// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/UnitSoftAvoidanceProcessor.h"
#include "DrawDebugHelpers.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h" // UWorld::IsNetMode / NM_Client

// CLIENT-ONLY multiplier for the soft-avoidance (push-back-onto-navmesh) force. Default 1.0 = FULL: unlike
// separation, this force keeps units ON the navmesh at corners/edges and largely AGREES with the reconciler
// (both push toward valid server-authoritative positions), so it is intentionally not weakened. Exposed for
// tuning only (lower it if soft-avoidance ever fights the reconciler).
static TAutoConsoleVariable<float> CVarRTS_ClientSoftAvoidanceForceScale(
	TEXT("net.RTS.Client.SoftAvoidanceForceScale"),
	1.0f,
	TEXT("Client-only multiplier for soft-avoidance (on-navmesh push) force. Default 1.0 (full). Keeps units on the mesh at corners."),
	ECVF_Default);

UUnitSoftAvoidanceProcessor::UUnitSoftAvoidanceProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UUnitSoftAvoidanceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassSoftAvoidanceTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSoftAvoidanceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun -= ExecutionInterval;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context.GetWorld());
	if (!NavSys)
	{
		return;
	}

	const UWorld* SoftWorld = Context.GetWorld();
	const float ClientSoftScale = (SoftWorld && SoftWorld->IsNetMode(NM_Client))
		? CVarRTS_ClientSoftAvoidanceForceScale.GetValueOnAnyThread() : 1.f;

	// Zaehlt die Einheiten, deren Projektion auf das Navigationsnetz scheitert - also die,
	// denen dieser Prozessor NICHT mehr helfen kann. Siehe Belegzeile am Ende.
	int32 AbandonedUnits = 0;
	// Einheiten, die erst die enge Projektion verfehlten und ueber den weiten Suchbereich doch noch
	// eine Rueckholkraft bekamen. Trennt "gerettet" von "endgueltig verloren".
	int32 RescuedUnits = 0;

	EntityQuery.ForEachEntityChunk(Context, [this, NavSys, &EntityManager, ClientSoftScale, &AbandonedUnits, &RescuedUnits](FMassExecutionContext& LocalContext)
	{
		const int32 Num = LocalContext.GetNumEntities();
		const auto Transforms = LocalContext.GetFragmentView<FTransformFragment>();
		const auto ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
		const auto CharacteristicsList = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

		for (int32 i = 0; i < Num; ++i)
		{
            FMassEntityHandle Entity = LocalContext.GetEntity(i);
			FVector Location = Transforms[i].GetTransform().GetLocation();
			const FMassAgentCharacteristicsFragment& Characteristics = CharacteristicsList[i];

			if (Characteristics.bIsFlying)
			{
				LocalContext.Defer().RemoveTag<FMassSoftAvoidanceTag>(Entity);
				continue;
			}

			const FVector ProjectionExtent(Characteristics.CapsuleRadius * 4.0f, Characteristics.CapsuleRadius * 4.0f, ZExtent);

            FNavLocation NavLoc;
            bool bOnNavMesh = NavSys->ProjectPointToNavigation(Location, NavLoc, ProjectionExtent);
            
            bool bHasTag = DoesEntityHaveTag(EntityManager, Entity, FMassSoftAvoidanceTag::StaticStruct());

            // Detect if projected poly is an obstacle (dirty area)
            bool bInDirtyArea = false;
            if (bOnNavMesh)
            {
                const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
                if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
                {
                    const uint32 PolyAreaID = Recast->GetPolyAreaID(NavLoc.NodeRef);
                    const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                    bInDirtyArea = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
                }
            }
            
            if (!bOnNavMesh || bHasTag || bInDirtyArea)
            {
                if (bOnNavMesh)
                {
                    FVector Target = NavLoc.Location;

                    // If in dirty area, search a nearby non-dirty projected point
                    if (bInDirtyArea)
                    {
                        static const float Radii[] = {100.f, 200.f, 400.f, 800.f};
                        static const int32 Slices = 12;
                        bool bFound = false;
                        for (float R : Radii)
                        {
                            for (int32 s = 0; s < Slices; ++s)
                            {
                                const float Angle = (2 * PI) * (float(s) / float(Slices));
                                const FVector Candidate = Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * R;
                                FNavLocation CandNav;
                                if (NavSys->ProjectPointToNavigation(Candidate, CandNav, ProjectionExtent))
                                {
                                    bool bCandDirty = false;
                                    if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
                                    {
                                        const uint32 AreaID = Recast->GetPolyAreaID(CandNav.NodeRef);
                                        const UClass* AreaClass = Recast->GetAreaClass(AreaID);
                                        bCandDirty = AreaClass && AreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
                                    }
                                    if (!bCandDirty)
                                    {
                                        Target = CandNav.Location;
                                        bFound = true;
                                        break;
                                    }
                                }
                            }
                            if (bFound) break;
                        }
                    }

                    FVector ToNavMesh = Target - Location;
                    ToNavMesh.Z = 0.f;
                    const float Distance = ToNavMesh.Size();
                    
                    if (Distance > 5.f || bHasTag || bInDirtyArea) 
                    {
                        const FVector PushForce = ToNavMesh.GetSafeNormal() * AvoidanceStrength * ClientSoftScale;
                        ForceList[i].Value += PushForce;
                        
                        if (Debug)
                        {
                            DrawDebugSphere(LocalContext.GetWorld(), Location + FVector(0,0,100.f), 20.f, 8, FColor::Blue, false, ExecutionInterval * 2.f);
                            DrawDebugLine(LocalContext.GetWorld(), Location + FVector(0,0,100.f), Target + FVector(0,0,100.f), FColor::Blue, false, ExecutionInterval * 2.f);
                            DrawDebugDirectionalArrow(LocalContext.GetWorld(), Location + FVector(0,0,100.f), Location + FVector(0,0,100.f) + ToNavMesh.GetSafeNormal() * 150.f, 50.f, FColor::Green, false, ExecutionInterval * 2.f, 0, 2.f);
                        }
                    }
                    
                    if (Distance < 10.f && !bInDirtyArea)
                    {
                         LocalContext.Defer().RemoveTag<FMassSoftAvoidanceTag>(Entity);
                    }
                }
                else
                {
                     // Die enge Projektion ist gescheitert: die Einheit steht weiter als vier Kapselradien
                     // vom naechsten begehbaren Punkt entfernt, also typischerweise mitten im Loch, das ein
                     // Gebaeude ins Netz stanzt.
                     //
                     // Frueher wurde hier NUR die Markierung entfernt und KEINE Kraft gesetzt - die Einheit
                     // bekam von diesem Prozessor nie wieder Hilfe und blieb dauerhaft stehen. Gemessen am
                     // 30.08. ueber vier volle Partien: 1061 bis 1353 Meldungen je Partie, und die Zeile ist
                     // auf hoechstens eine je Sekunde gedrosselt - es standen also fast die ganze Partie
                     // ueber Einheiten fest. In der langsamsten Partie beherrschten [NavAussen] und
                     // [RunStall] das Log, und sie rechnete nur 898 statt 1500 Spielsekunden ab.
                     //
                     // Jetzt wird ein zweites Mal projiziert, mit weitem Suchbereich. Findet sich irgendwo
                     // ein begehbarer Punkt, bekommt die Einheit die Rueckholkraft dorthin - ein langer Weg
                     // hinaus ist allemal besser als dauerhaft im Gebaeude zu stecken. Erst wenn auch das
                     // scheitert, wird wie bisher aufgegeben.
                     const float WeiteReichweite = FMath::Max(1500.f, Characteristics.CapsuleRadius * 40.0f);
                     const FVector WeiterBereich(WeiteReichweite, WeiteReichweite, FMath::Max(ZExtent, 1000.f));

                     FNavLocation WeitNavLoc;
                     if (NavSys->ProjectPointToNavigation(Location, WeitNavLoc, WeiterBereich))
                     {
                         FVector HinausRichtung = WeitNavLoc.Location - Location;
                         HinausRichtung.Z = 0.f;
                         if (!HinausRichtung.IsNearlyZero())
                         {
                             ForceList[i].Value += HinausRichtung.GetSafeNormal() * AvoidanceStrength * ClientSoftScale;
                             ++RescuedUnits;
                         }
                     }
                     else
                     {
                         ++AbandonedUnits;
                     }

                     LocalContext.Defer().RemoveTag<FMassSoftAvoidanceTag>(Entity);
                     if (Debug)
                     {
                         DrawDebugSphere(LocalContext.GetWorld(), Location + FVector(0,0,150.f), 30.f, 8, FColor::Red, false, ExecutionInterval * 2.f);
                     }
                }
            }
			}
	});

	// Belegzeile, hoechstens einmal pro Sekunde: wie viele Einheiten in diesem Takt ausserhalb
	// des Navigationsnetzes standen, ohne dass eine Rueckholkraft moeglich war. Nach dem
	// Spawn-Fix (AUnitBase::SpawnUnitsFromParameters zieht jede Spawnstelle auf das Netz)
	// muss diese Zahl deutlich kleiner sein als vorher.
	if (AbandonedUnits > 0 || RescuedUnits > 0)
	{
		static thread_local double LetzteMeldung = 0.0;
		const double Jetzt = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;
		if (Jetzt - LetzteMeldung > 1.0 || Jetzt < LetzteMeldung)
		{
			LetzteMeldung = Jetzt;
			UE_LOG(LogTemp, Warning, TEXT("[NavAussen] %d Einheiten ausserhalb des Navigationsnetzes - keine Rueckholkraft moeglich (%d ueber den weiten Suchbereich gerettet)"), AbandonedUnits, RescuedUnits);
		}
	}
}
