// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitApplyMassMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassCommonTypes.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Characters/Unit/AbilityUnit.h"
#include "EngineUtils.h"

// Patt-Aufloesung an Hindernissen - die Begruendung steht am Feld bTangentialAvoidance
// im Header, damit sie dort steht, wo der Schalter sichtbar ist.
FVector UUnitApplyMassMovementProcessor::RedirectedAvoidanceForce(const FVector& Ausweichkraft, const FVector& Sollgeschwindigkeit) const
{
	if (!bTangentialAvoidance) return Ausweichkraft;
	if (Ausweichkraft.IsNearlyZero() || Sollgeschwindigkeit.IsNearlyZero()) return Ausweichkraft;

	const FVector Laufrichtung = Sollgeschwindigkeit.GetSafeNormal();
	const float FrontalAnteil = FVector::DotProduct(Ausweichkraft, Laufrichtung);
	if (FrontalAnteil >= 0.f) return Ausweichkraft;   // schiebt mit, kein Patt

	const FVector FrontalTeil = Laufrichtung * FrontalAnteil;
	FVector SeitlicherTeil = Ausweichkraft - FrontalTeil;
	if (SeitlicherTeil.IsNearlyZero())
	{
		// Exakt frontal - dann gibt die Geometrie keine Seite vor. Immer dieselbe Seite waehlen,
		// damit zwei Einheiten sich nicht gegenseitig hin und her schieben.
		SeitlicherTeil = FVector(-Laufrichtung.Y, Laufrichtung.X, 0.f);
	}

	// Die Gesamtstaerke bleibt erhalten, sie wandert nur in die seitliche Richtung.
	const FVector Umlenkung = SeitlicherTeil.GetSafeNormal() * Ausweichkraft.Size2D();
	return FrontalTeil * FMath::Clamp(AvoidanceBrakeFactor, 0.f, 1.f) + Umlenkung;
}
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"

// NOTE: client-side avoidance weakening is now done PER-PROCESSOR at the source (UnitSeparationProcessor,
// UnitSoftAvoidanceProcessor, UnitMovingAvoidanceProcessor) so separation (the lateral-push jitter culprit)
// can be weakened hard while soft-avoidance (keeps units ON the navmesh at corners) stays full. The former
// blanket consumer scale (net.RTS.Client.AvoidanceForceScale) was removed in favor of that split.

UUnitApplyMassMovementProcessor::UUnitApplyMassMovementProcessor(): EntityQuery()
{

	//ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	//ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UUnitApplyMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly); 
	
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any); 
	EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
	EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
	
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
	//EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

	EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
	
	EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
	// LUX-ANPASSUNG (16.08.2026): sperrt auf FMassStopWhileAimingTag statt auf
	// FMassRotateToMouseTag, damit die direkt gesteuerte CameraUnit beim Zielen laufen
	// darf. Alle anderen Einheiten tragen beide Tags -> Verhalten unveraendert.
	// Original: AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStopWhileAimingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
	
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	
	EntityQuery.RegisterWithProcessor(*this);

	ClientEntityQuery.Initialize(EntityManager);
	ClientEntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	ClientEntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	//ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	ClientEntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly);
	ClientEntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	// Mirror relevant state tags on client to limit work to moving entities
	ClientEntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	ClientEntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any); 
	ClientEntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
	ClientEntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
	
	ClientEntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
	//ClientEntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::Any);

	ClientEntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::Any);
	ClientEntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::Any);
	
	ClientEntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);  
	ClientEntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	ClientEntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
	ClientEntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	ClientEntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
	// LUX-ANPASSUNG (16.08.2026): sperrt auf FMassStopWhileAimingTag statt auf
	// FMassRotateToMouseTag, damit die direkt gesteuerte CameraUnit beim Zielen laufen
	// darf. Alle anderen Einheiten tragen beide Tags -> Verhalten unveraendert.
	// Original: AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
	ClientEntityQuery.AddTagRequirement<FMassStopWhileAimingTag>(EMassFragmentPresence::None);
	ClientEntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);
	
	ClientEntityQuery.RegisterWithProcessor(*this);
}

void UUnitApplyMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());
    if (DeltaTime <= 0.0f)
    {
        return;
    }

	UWorld* World = GetWorld();
	if (!World) return;
	
    if (World->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UUnitApplyMassMovementProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

    ClientEntityQuery.ForEachEntityChunk(Context, [this, DeltaTime, &EntityManager](FMassExecutionContext& LocalContext)
    {
        const int32 NumEntities = LocalContext.GetNumEntities();
        if (NumEntities == 0) return;

        const FMassMovementParameters& MovementParams = LocalContext.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = LocalContext.GetFragmentView<FMassSteeringFragment>();
        const TArrayView<FTransformFragment> LocationList = LocalContext.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = LocalContext.GetMutableFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharacteristicsList = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = LocalContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = LocalContext.GetFragmentView<FMassActorFragment>();

        const bool bFreezeXY = LocalContext.DoesArchetypeHaveTag<FMassStateStopXYMovementTag>();
        
        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
            const FMassAIStateFragment& AIState = AIStateList[EntityIndex];

            if (!AIState.CanMove)
            {
                Velocity.Value = FVector::ZeroVector;
                Force.Value = FVector::ZeroVector;
                continue;
            }

            if (const AActor* Actor = ActorList[EntityIndex].Get())
            {
                if (Actor->GetName().Contains(TEXT("ConstructionSite")) || Actor->GetName().Contains(TEXT("ConstructionUnit")))
                {
                     if (!Velocity.Value.IsNearlyZero() || !Steering.DesiredVelocity.IsNearlyZero() || !Force.Value.IsNearlyZero())
                     {
                        UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] UnitApplyMassMovementProcessor Client: %s - Velocity: %s, Steering: %s, Force: %s"), 
                            *Actor->GetName(), *Velocity.Value.ToString(), *Steering.DesiredVelocity.ToString(), *Force.Value.ToString());
                     }
                }
            }

            // DIAGNOSE (17.08.2026): laeuft dieser Applier auf dem Client fuer die direkt gesteuerte
            // Einheit ueberhaupt? Silvan meldet nach dem Entfernen meines Velocity-Schreibens wieder
            // "Schlittschuhe", also Velocity=0 - das waere nur erklaerbar, wenn dieser Zweig die
            // Entity nicht erreicht oder sie hier ausgebremst wird. Nur die direkt gesteuerte
            // Einheit, hoechstens dreimal pro Sekunde.
            if (DoesEntityHaveTag(EntityManager, LocalContext.GetEntity(EntityIndex),
                                  FMassDirectControlTag::StaticStruct()))
            {
                static double LetzteAusgabe = 0.0;
                const double Jetzt = FPlatformTime::Seconds();
                if (Jetzt - LetzteAusgabe > 0.33)
                {
                    LetzteAusgabe = Jetzt;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[ApplierDiag] CLIENT laeuft: Vel=%.0f Desired=%.0f Force=%.0f MaxSpeed=%.0f Accel=%.0f CanMove=%d"),
                        Velocity.Value.Size2D(), Steering.DesiredVelocity.Size2D(), Force.Value.Size2D(),
                        MovementParams.MaxSpeed, MovementParams.MaxAcceleration, (int32)AIState.CanMove);
                }
            }

            const float OriginalZVelocity = Velocity.Value.Z;
            const FVector DesiredVelocity = Steering.DesiredVelocity;
            const FVector AvoidanceForce = Force.Value;
            const float MaxSpeed = MovementParams.MaxSpeed;
            const float Acceleration = MovementParams.MaxAcceleration;

            const FVector CurrentHorizontalVelocity(Velocity.Value.X, Velocity.Value.Y, 0.f);
            const FVector DesiredHorizontalVelocity(DesiredVelocity.X, DesiredVelocity.Y, 0.f);
            // Avoidance Force is already weakened per-source on the client (separation/moving scaled down,
            // soft-avoidance kept full); integrate it as-is here.
            const FVector HorizontalAvoidanceForce(AvoidanceForce.X, AvoidanceForce.Y, 0.f);

            FVector AccelInput = (DesiredHorizontalVelocity - CurrentHorizontalVelocity);
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);
            const FVector EffectiveAvoidanceForce = RedirectedAvoidanceForce(HorizontalAvoidanceForce, DesiredHorizontalVelocity);
            FVector HorizontalVelocityDelta = (AccelInput + EffectiveAvoidanceForce) * DeltaTime * 4.f;

            FVector NewHorizontalVelocity = CurrentHorizontalVelocity + HorizontalVelocityDelta;
            NewHorizontalVelocity = NewHorizontalVelocity.GetClampedToMaxSize(MaxSpeed);

            if (bFreezeXY)
            {
                Velocity.Value = FVector(0.f, 0.f, OriginalZVelocity);
            }
            else
            {
                Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);
            }

            const FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;

            UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(LocalContext.GetWorld());
            const FMassAgentCharacteristicsFragment& Characteristics = CharacteristicsList[EntityIndex];

            if (NavSys && !Characteristics.bIsFlying)
            {
                const FVector ProjectionExtent(Characteristics.CapsuleRadius * 4.0f, Characteristics.CapsuleRadius * 4.0f, SoftAvoidanceZExtent);

                FNavLocation ProjectedLocation;
                // Use capsule-based extent to detect if we are on the mesh or inside a DirtyArea
                bool bNeedsAvoidance = false;
                if (!NavSys->ProjectPointToNavigation(NewLocation, ProjectedLocation, ProjectionExtent) ||
                    FVector::DistSquared2D(NewLocation, ProjectedLocation.Location) > FMath::Square(5.f))
                {
                    bNeedsAvoidance = true;
                }
                else
                {
                    // NEU: Check, ob der projizierte Punkt in einer UNavArea_Obstacle (Energy Wall) liegt!
                    if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
                    {
                        const uint32 PolyAreaID = Recast->GetPolyAreaID(ProjectedLocation.NodeRef);
                        const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                        if (PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass()))
                        {
                            bNeedsAvoidance = true;
                        }
                    }
                }

                if (bNeedsAvoidance)
                {
                    LocalContext.Defer().AddTag<FMassSoftAvoidanceTag>(LocalContext.GetEntity(EntityIndex));
                }
            }

            CurrentTransform.SetTranslation(NewLocation);

            Force.Value = FVector::ZeroVector;
        }
    });
}

void UUnitApplyMassMovementProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

    // Blockiert-Zaehler: Einheiten, die laufen WOLLEN (Sollgeschwindigkeit gesetzt), sich aber
    // faktisch nicht bewegen. Das ist das Symptom "haengt am Gebaeude fest", unabhaengig davon,
    // ob die Einheit gerade einen berechneten Pfad ablaeuft - der StuckTimer im
    // UnitStateProcessor greift naemlich nur im Wegpunktmodus und sieht diesen Fall nicht.
    int32 BlockedUnits = 0;
    // Kraftbilanz der blockierten Einheiten - siehe Kommentar an der Zaehlstelle.
    double AvoidanceForceSum = 0.0;
    double CosineSum = 0.0;
    int32 WithAvoidanceForce = 0;
    int32 OpposingForceCount = 0;
    // Zweite Erklaerung, die geprueft werden muss: der Tag FMassStateStopXYMovementTag setzt
    // die Geschwindigkeit hart auf null (bFreezeXY weiter unten) - unabhaengig von Soll und
    // Pfad. Traegt eine blockierte Einheit ihn, ist die Ausweichkraft gar nicht die Ursache.
    int32 BlockedWithFreeze = 0;
    double DesiredSpeedSum = 0.0;
    double ResultSpeedSum = 0.0;
    double PreviousSpeedSum = 0.0;
    double ForceVsTravelSum = 0.0;
    double DesiredVsTravelSum = 0.0;
    int32 WithTravel = 0;
    double AccelSum = 0.0;
    double DeltaTimeSum = 0.0;
    double StepSum = 0.0;
    int32 MovingUnits = 0;
    // Zusaetzlich getrennt: frisch gespawnte Einheiten (juenger als 15 s). Genau die bleiben
    // laut Beobachtung am Gebaeude haengen. Die Gesamtquote allein taugt dafuer nicht - sie
    // enthaelt auch normales Gedraenge im Kampf, das nichts mit dem Spawn zu tun hat.
    int32 BlockedYoung = 0;
    int32 MovingYoung = 0;

    // Dritte, entscheidende Messgroesse: Einheiten, die sich ueber laengere Zeit kaum von der
    // Stelle bewegen - UNABHAENGIG davon, ob sie laufen wollen.
    //
    // Die Quote oben erfasst nur Einheiten mit gesetzter Sollgeschwindigkeit. Wer am Gebaeude
    // klebt, weil er gar keinen Bewegungsauftrag mehr bekommt (Zustand haengt statt Physik),
    // hat Sollgeschwindigkeit null und taucht dort NICHT auf. Genau dieser Fall wird hier
    // gezaehlt: Position je Einheit merken und pruefen, wie lange sie sich nicht bewegt hat.
    const double JetztWelt = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;
    int32 LongStationary = 0;
    int32 Observed = 0;
    // Getrennt gezaehlt: Gebaeude stehen naturgemaess still und wuerden die Zahl sonst
    // aufblaehen. Nur der Rest ist ein Befund.
    int32 StationaryBuildings = 0;
    // Und getrennt die Arbeiter: sie sollen nie laenger stillstehen, waehrend eine Kampfeinheit
    // auf Wachposition das voellig zu Recht tut. Nur diese Zahl trifft die Beobachtung
    // "Arbeiter bleiben am Gebaeude haengen".
    int32 StationaryWorkers = 0;
    int32 WorkersTotal = 0;
    // Und der eigentliche Diagnosewert: in WELCHEM Zustand stehen die festgefahrenen
    // Arbeiter? Erst das sagt, wo der Fehler sitzt - im Bauen, im Sammeln, im Ruecklauf.
    TMap<uint8, int32> StalledStateCounts;
    // Die eigentliche Kennzahl. Ein Arbeiter im Zustand Build oder ResourceExtraction steht
    // voellig zu Recht still - er baut bzw. sammelt gerade, und diese beiden machen 56 % aller
    // Stillstaende aus. Sie mitzuzaehlen verwaessert jede Verbesserung um mehr als die Haelfte.
    // Gezaehlt wird deshalb nur, wer sich BEWEGEN WILL und trotzdem steht.
    int32 MovingWorkers = 0;
    int32 StalledMovingWorkers = 0;

    // Abstand zum naechsten Gebaeude - fuer die Haenger UND fuer alle laufwilligen Arbeiter.
    // Ohne die zweite Verteilung sagt die erste nichts: sind ohnehin die meisten Arbeiter in
    // Gebaeudenaehe unterwegs, waere 'viele Haenger stehen nahe Gebaeuden' kein Befund.
    if (Context.GetWorld() && (JetztWelt - BuildingLocationsStamp > 1.0 || JetztWelt < BuildingLocationsStamp))
    {
        BuildingLocationsStamp = JetztWelt;
        BuildingLocations.Reset();
        for (TActorIterator<ABuildingBase> It(Context.GetWorld()); It; ++It)
        {
            if (*It) BuildingLocations.Add(It->GetActorLocation());
        }
    }
    int32 StalledNear = 0, StalledMid = 0, StalledFar = 0;
    // Kraft getrennt nach Gebaeudenaehe: der DynamicObstacleRegProcessor traegt grosse
    // Objekte als KRANZ vieler kleiner Teilhindernisse ein (je 100 Radius auf dem Umfang),
    // und im MovingAvoidance bekommt jeder Kontakt seine eigene Separationskraft, die alle
    // aufaddiert werden. Dicht am Gebaeude muesste die Kraft deshalb deutlich groesser sein
    // als im freien Feld - wenn das stimmt, ist der Kranz die Ursache.
    double ForceNearSum = 0.0; int32 ForceNearCount = 0;
    double ForceFarSum = 0.0; int32 ForceFarCount = 0;
    int32 AllNear = 0, AllMid = 0, AllFar = 0;

    EntityQuery.ForEachEntityChunk(Context, [this, DeltaTime, JetztWelt, &DesiredSpeedSum, &ResultSpeedSum, &PreviousSpeedSum, &ForceVsTravelSum, &DesiredVsTravelSum, &WithTravel, &AccelSum, &DeltaTimeSum, &StepSum, &BlockedWithFreeze, &AvoidanceForceSum, &CosineSum, &WithAvoidanceForce, &OpposingForceCount, &BlockedUnits, &MovingUnits, &BlockedYoung, &MovingYoung, &LongStationary, &Observed, &StationaryBuildings, &StationaryWorkers, &WorkersTotal, &StalledStateCounts, &MovingWorkers, &StalledMovingWorkers, &StalledNear, &StalledMid, &StalledFar, &ForceNearSum, &ForceNearCount, &ForceFarSum, &ForceFarCount, &AllNear, &AllMid, &AllFar](FMassExecutionContext& LocalContext)
    {
        const int32 NumEntities = LocalContext.GetNumEntities();
        if (NumEntities == 0) return;

        const FMassMovementParameters& MovementParams = LocalContext.GetConstSharedFragment<FMassMovementParameters>();
        const TConstArrayView<FMassSteeringFragment> SteeringList = LocalContext.GetFragmentView<FMassSteeringFragment>();
        const TArrayView<FTransformFragment> LocationList = LocalContext.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FMassForceFragment> ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
        const TArrayView<FMassVelocityFragment> VelocityList = LocalContext.GetMutableFragmentView<FMassVelocityFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharacteristicsList = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassAIStateFragment> AIStateList = LocalContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = LocalContext.GetFragmentView<FMassActorFragment>();

        const bool bFreezeXY = LocalContext.DoesArchetypeHaveTag<FMassStateStopXYMovementTag>();

        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
            const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
            FMassForceFragment& Force = ForceList[EntityIndex];
            FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
            const FMassAIStateFragment& AIState = AIStateList[EntityIndex];

            if (!AIState.CanMove)
            {
                Velocity.Value = FVector::ZeroVector;
                Force.Value = FVector::ZeroVector;
                continue;
            }

            if (const AActor* Actor = ActorList[EntityIndex].Get())
            {
                if (Actor->GetName().Contains(TEXT("ConstructionSite")) || Actor->GetName().Contains(TEXT("ConstructionUnit")))
                {
                     if (!Velocity.Value.IsNearlyZero() || !Steering.DesiredVelocity.IsNearlyZero() || !Force.Value.IsNearlyZero())
                     {
                        UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] UnitApplyMassMovementProcessor Server: %s - Velocity: %s, Steering: %s, Force: %s"), 
                            *Actor->GetName(), *Velocity.Value.ToString(), *Steering.DesiredVelocity.ToString(), *Force.Value.ToString());
                     }
                }
            }

            const float OriginalZVelocity = Velocity.Value.Z;
            const FVector DesiredVelocity = Steering.DesiredVelocity;
            const FVector AvoidanceForce = Force.Value;
            const float MaxSpeed = MovementParams.MaxSpeed;
            const float Acceleration = MovementParams.MaxAcceleration;

            const FVector CurrentHorizontalVelocity(Velocity.Value.X, Velocity.Value.Y, 0.f);
            const FVector DesiredHorizontalVelocity(DesiredVelocity.X, DesiredVelocity.Y, 0.f);
            const FVector HorizontalAvoidanceForce(AvoidanceForce.X, AvoidanceForce.Y, 0.f);

            FVector AccelInput = (DesiredHorizontalVelocity - CurrentHorizontalVelocity);
            AccelInput = AccelInput.GetClampedToMaxSize(Acceleration);
            const FVector EffectiveAvoidanceForce = RedirectedAvoidanceForce(HorizontalAvoidanceForce, DesiredHorizontalVelocity);
            FVector HorizontalVelocityDelta = (AccelInput + EffectiveAvoidanceForce) * DeltaTime * 4.f;

            FVector NewHorizontalVelocity = CurrentHorizontalVelocity + HorizontalVelocityDelta;
            NewHorizontalVelocity = NewHorizontalVelocity.GetClampedToMaxSize(MaxSpeed);

            if (bFreezeXY)
            {
                Velocity.Value = FVector(0.f, 0.f, OriginalZVelocity);
            }
            else
            {
                Velocity.Value = FVector(NewHorizontalVelocity.X, NewHorizontalVelocity.Y, OriginalZVelocity);
            }

            const FVector CurrentLocation = CurrentTransform.GetLocation();
            FVector NewLocation = CurrentLocation + Velocity.Value * DeltaTime;

            UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(LocalContext.GetWorld());
            const FMassAgentCharacteristicsFragment& Characteristics = CharacteristicsList[EntityIndex];

            if (NavSys && !Characteristics.bIsFlying)
            {
                const FVector ProjectionExtent(Characteristics.CapsuleRadius * 4.0f, Characteristics.CapsuleRadius * 4.0f, SoftAvoidanceZExtent);

                FNavLocation ProjectedLocation;
                // Use capsule-based extent to detect if we are on the mesh or inside a DirtyArea
                bool bNeedsAvoidance = false;
                if (!NavSys->ProjectPointToNavigation(NewLocation, ProjectedLocation, ProjectionExtent) || 
                    FVector::DistSquared2D(NewLocation, ProjectedLocation.Location) > FMath::Square(5.f))
                {
                    bNeedsAvoidance = true;
                }
                else
                {
                    // NEU: Check, ob der projizierte Punkt in einer UNavArea_Obstacle (Energy Wall) liegt!
                    if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
                    {
                        const uint32 PolyAreaID = Recast->GetPolyAreaID(ProjectedLocation.NodeRef);
                        const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                        if (PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass()))
                        {
                            bNeedsAvoidance = true;
                        }
                    }
                }

                if (bNeedsAvoidance)
                {
                    LocalContext.Defer().AddTag<FMassSoftAvoidanceTag>(LocalContext.GetEntity(EntityIndex));
                }
            }

            // Will die Einheit laufen, kommt aber nicht vom Fleck?
            if (DesiredVelocity.SizeSquared2D() > FMath::Square(50.f))
            {
                const bool bSteht = FVector::DistSquared2D(NewLocation, CurrentLocation) < FMath::Square(0.5f);
                ++MovingUnits;
                if (bSteht)
                {
                    ++BlockedUnits;
                    if (bFreezeXY)
                    {
                        ++BlockedWithFreeze;
                    }

                    // Warum steht sie? Die Ausweichkraft geht direkt in die Beschleunigung ein
                    // (AccelInput + HorizontalAvoidanceForce). Zeigt sie gegen die Laufrichtung,
                    // heben sich beide auf und die Einheit tritt auf der Stelle. Der Kosinus
                    // zwischen Sollrichtung und Ausweichkraft macht genau das sichtbar:
                    // -1 = exakt entgegengesetzt, +1 = gleiche Richtung.
                    // Gemessen wird die WIRKSAME Kraft, nicht die rohe: die Umlenkung veraendert
                    // nur die wirksame, und genau deren Richtung entscheidet, ob die Einheit
                    // ausgebremst wird. Die rohe Kraft zu messen konnte den Effekt der
                    // Korrektur gar nicht zeigen.
                    if (!EffectiveAvoidanceForce.IsNearlyZero())
                    {
                        const float Kosinus = FVector::DotProduct(
                            DesiredHorizontalVelocity.GetSafeNormal(), EffectiveAvoidanceForce.GetSafeNormal());
                        AvoidanceForceSum += EffectiveAvoidanceForce.Size2D();
                        // Die Geschwindigkeiten selbst: Soll, das berechnete Ergebnis und die
                        // Beschleunigung. Ist NewHorizontalVelocity spuerbar groesser als null,
                        // die Position aber unveraendert, wird die Bewegung ausserhalb dieses
                        // Prozessors wieder zurueckgenommen.
                        DesiredSpeedSum += DesiredHorizontalVelocity.Size2D();
                        // Zwei Winkel trennen die Faelle: zeigt die AUSWEICHKRAFT gegen die
                        // laufende Bewegung, bremst die Avoidance. Zeigt das SOLL gegen die
                        // laufende Bewegung, springt die Zielrichtung hin und her und die
                        // Beschleunigung loescht die eigene Fahrt aus - zwei ganz
                        // verschiedene Ursachen mit ganz verschiedenen Korrekturen.
                        if (!CurrentHorizontalVelocity.IsNearlyZero())
                        {
                            const FVector Fahrt = CurrentHorizontalVelocity.GetSafeNormal();
                            ForceVsTravelSum += FVector::DotProduct(Fahrt, EffectiveAvoidanceForce.GetSafeNormal());
                            DesiredVsTravelSum += FVector::DotProduct(Fahrt, DesiredHorizontalVelocity.GetSafeNormal());
                            ++WithTravel;
                        }

                        // Verhaeltnis der Kraefte: die Ausweichkraft geht ungefiltert in
                        // dieselbe Summe wie die Zielbeschleunigung. Ist sie ein Vielfaches
                        // davon, kann sie die Zielansteuerung dauerhaft neutralisieren - dann
                        // ist eine Begrenzung der Kraft der richtige Hebel, nicht ihre Richtung.
                        AccelSum += AccelInput.Size2D();
                        // Der Zeitschritt selbst: die Geschwindigkeit wird mit
                        // (AccelInput + Kraft) * DeltaTime * 4 fortgeschrieben. Wird DeltaTime
                        // gross - viele Partien parallel, Zeitdehnung, Lastspitze -, schiesst
                        // dieser Schritt ueber die Sollgeschwindigkeit hinaus und die Einheit
                        // pendelt, statt sich einzuschwingen.
                        DeltaTimeSum += DeltaTime;
                        StepSum += (AccelInput + EffectiveAvoidanceForce).Size2D() * DeltaTime * 4.f;
                        ResultSpeedSum += NewHorizontalVelocity.Size2D();
                        PreviousSpeedSum += CurrentHorizontalVelocity.Size2D();
                        CosineSum += Kosinus;
                        ++WithAvoidanceForce;
                        if (Kosinus < -0.5f)
                        {
                            ++OpposingForceCount;
                        }
                    }
                }

                // Alter ueber den zugehoerigen Actor: frisch gespawnt heisst hier juenger als 15 s.
                if (const AActor* Besitzer = ActorList[EntityIndex].Get())
                {
                    if (Besitzer->GetGameTimeSinceCreation() < 15.f)
                    {
                        ++MovingYoung;
                        if (bSteht)
                        {
                            ++BlockedYoung;
                        }
                    }
                }
            }

            // Standwache: je Einheit die letzte Position und den Zeitpunkt merken. Wer sich
            // seit mehr als 30 Weltsekunden um weniger als 150 uu bewegt hat, gilt als
            // festgefahren - unabhaengig von Bewegungswillen und Zustand.
            {
                ++Observed;
                const AAbilityUnit* AlsEinheitFuerZustand = Cast<AAbilityUnit>(ActorList[EntityIndex].Get());
                float LastBuildingDistance = TNumericLimits<float>::Max();
                const bool bIsWorker = Cast<AWorkingUnitBase>(ActorList[EntityIndex].Get()) != nullptr;
                bool bWantsToMove = false;
                if (bIsWorker)
                {
                    ++WorkersTotal;
                    if (AlsEinheitFuerZustand)
                    {
                        switch (AlsEinheitFuerZustand->GetUnitState())
                        {
                        case UnitData::Run:
                        case UnitData::Patrol:
                        case UnitData::PatrolRandom:
                        case UnitData::GoToBase:
                        case UnitData::GoToBuild:
                        case UnitData::GoToRepair:
                        case UnitData::GoToResourceExtraction:
                        case UnitData::Chase:
                            bWantsToMove = true;
                            break;
                        default:
                            break;
                        }
                    }
                    if (bWantsToMove)
                    {
                        ++MovingWorkers;
                        float NaechstesGebaeude = TNumericLimits<float>::Max();
                        for (const FVector& Ort : BuildingLocations)
                        {
                            NaechstesGebaeude = FMath::Min(NaechstesGebaeude, (float)FVector::Dist2D(NewLocation, Ort));
                        }
                        if (NaechstesGebaeude < 300.f)       ++AllNear;
                        else if (NaechstesGebaeude < 800.f)  ++AllMid;
                        else                                 ++AllFar;
                        LastBuildingDistance = NaechstesGebaeude;
                    }
                }
                const FMassEntityHandle Wer = LocalContext.GetEntity(EntityIndex);
                FStationaryWatch& Wache = StationaryWatches.FindOrAdd(Wer);
                if (Wache.Zeitpunkt <= 0.0 || FVector::DistSquared2D(NewLocation, Wache.Ort) > FMath::Square(150.f))
                {
                    Wache.Ort = NewLocation;
                    Wache.Zeitpunkt = JetztWelt;
                }
                else if (JetztWelt - Wache.Zeitpunkt > 30.0)
                {
                    ++LongStationary;
                    if (Cast<ABuildingBase>(ActorList[EntityIndex].Get()))
                    {
                        ++StationaryBuildings;
                    }
                    if (bIsWorker)
                    {
                        ++StationaryWorkers;
                        if (bWantsToMove)
                        {
                            ++StalledMovingWorkers;
                            if (LastBuildingDistance < 300.f)       ++StalledNear;
                            else if (LastBuildingDistance < 800.f)  ++StalledMid;
                            else                                      ++StalledFar;
                            if (LastBuildingDistance < 300.f)
                            {
                                ForceNearSum += HorizontalAvoidanceForce.Size2D();
                                ++ForceNearCount;
                            }
                            else if (LastBuildingDistance >= 800.f)
                            {
                                ForceFarSum += HorizontalAvoidanceForce.Size2D();
                                ++ForceFarCount;
                            }
                        }
                        if (const AAbilityUnit* AlsEinheit = Cast<AAbilityUnit>(ActorList[EntityIndex].Get()))
                        {
                            StalledStateCounts.FindOrAdd(static_cast<uint8>(AlsEinheit->GetUnitState().GetValue()))++;
                        }
                    }
                }
            }

            CurrentTransform.SetTranslation(NewLocation);

            Force.Value = FVector::ZeroVector;
        }
    });

    // Belegzeile, hoechstens einmal pro Sekunde: Anteil der Einheiten, die laufen wollen, aber
    // stehen. Steigt diese Zahl, klemmt es an der Bewegung - genau das beobachtete Bild.
    // Belegzeile der Standwache - eigene Drosselung, damit sie auch dann erscheint, wenn
    // gerade keine laufwillige Einheit blockiert ist.
    if (LongStationary > 0)
    {
        static thread_local double LetzteStandMeldung = 0.0;
        if (JetztWelt - LetzteStandMeldung > 5.0 || JetztWelt < LetzteStandMeldung)
        {
            LetzteStandMeldung = JetztWelt;

            if (MovingWorkers > 0)
            {


                if (ForceNearCount > 0 || ForceFarCount > 0)
                {
                }
            }

            // Aufschluesselung nach Zustand, absteigend nicht noetig - die Zahlen sind klein.
            // Zustaende als Klartext: eine rohe Enum-Zahl im Log muss spaeter jemand von Hand
            // nachschlagen und bleibt erfahrungsgemaess unentschluesselt liegen.
            const UEnum* ZustandsEnum = StaticEnum<UnitData::EState>();
            FString Aufschluesselung;
            for (const TPair<uint8, int32>& Eintrag : StalledStateCounts)
            {
                const FString Name = ZustandsEnum
                    ? ZustandsEnum->GetNameStringByValue(Eintrag.Key)
                    : FString::FromInt(Eintrag.Key);
                Aufschluesselung += FString::Printf(TEXT("%s=%d "), *Name, Eintrag.Value);
            }
            if (!Aufschluesselung.IsEmpty())
            {
            }
        }
    }

    if (MovingUnits > 0 && BlockedUnits > 0)
    {
        static thread_local double LetzteMeldung = 0.0;
        const double Jetzt = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;
        if (Jetzt - LetzteMeldung > 1.0 || Jetzt < LetzteMeldung)
        {
            LetzteMeldung = Jetzt;

            if (WithAvoidanceForce > 0)
            {


                if (WithTravel > 0)
                {


                    UE_LOG(LogTemp, Warning, TEXT("[Zeitschritt] DeltaTime %.3f s, Tempoaenderung je Takt %.0f (Soll %.0f, vorher %.0f)"),
                        DeltaTimeSum / WithAvoidanceForce, StepSum / WithAvoidanceForce,
                        DesiredSpeedSum / WithAvoidanceForce, PreviousSpeedSum / WithAvoidanceForce);
                }
            }
        }
    }
}