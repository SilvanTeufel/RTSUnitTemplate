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

	EntityQuery.ForEachEntityChunk(Context, [this, NavSys, &EntityManager](FMassExecutionContext& LocalContext)
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
                        const FVector PushForce = ToNavMesh.GetSafeNormal() * AvoidanceStrength;
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
                     LocalContext.Defer().RemoveTag<FMassSoftAvoidanceTag>(Entity);
                     if (Debug)
                     {
                         DrawDebugSphere(LocalContext.GetWorld(), Location + FVector(0,0,150.f), 30.f, 8, FColor::Red, false, ExecutionInterval * 2.f);
                     }
                }
            }
		}
	});
}
