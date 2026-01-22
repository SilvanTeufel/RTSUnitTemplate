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

			const FVector ProjectionExtent(Characteristics.CapsuleRadius * 2.f, Characteristics.CapsuleRadius * 2.f, ZExtent);

            FNavLocation NavLoc;
            bool bOnNavMesh = NavSys->ProjectPointToNavigation(Location, NavLoc, ProjectionExtent);
            
            bool bHasTag = DoesEntityHaveTag(EntityManager, Entity, FMassSoftAvoidanceTag::StaticStruct());
            
            if (!bOnNavMesh || bHasTag)
            {
                if (bOnNavMesh)
                {
                    FVector ToNavMesh = NavLoc.Location - Location;
                    ToNavMesh.Z = 0.f;
                    float Distance = ToNavMesh.Size();
                    
                    if (Distance > 5.f || bHasTag) 
                    {
                        FVector PushForce = ToNavMesh.GetSafeNormal() * AvoidanceStrength;
                        ForceList[i].Value += PushForce;
                        
                        if (Debug)
                        {
                            DrawDebugSphere(LocalContext.GetWorld(), Location + FVector(0,0,100.f), 20.f, 8, FColor::Blue, false, ExecutionInterval * 2.f);
                            DrawDebugLine(LocalContext.GetWorld(), Location + FVector(0,0,100.f), NavLoc.Location + FVector(0,0,100.f), FColor::Blue, false, ExecutionInterval * 2.f);
                        }
                    }
                    
                    if (Distance < 10.f)
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
