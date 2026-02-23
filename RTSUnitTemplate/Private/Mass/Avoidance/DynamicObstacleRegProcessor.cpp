// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/DynamicObstacleRegProcessor.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassNavigationFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "Mass/UnitMassTag.h"
#include "NavigationSystem.h"


namespace
{
    static constexpr float InitialCellSize = 100.f;
    static constexpr float LifeTime = 3.0f;
    static constexpr float LargeRadiusThreshold = InitialCellSize/2.f;
    static constexpr float SubObstacleRadius = InitialCellSize;
    static constexpr float ClusteringDistanceThreshold = 150.f;
    
    const FColor BoxColor_Single = FColor::Red;
    const FColor SubBoxColor_Circle = FColor::Orange;
    const FColor SubBoxColor_Merged = FColor::Yellow;
}


UDynamicObstacleRegProcessor::UDynamicObstacleRegProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
}

void UDynamicObstacleRegProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	auto SetupQuery = [&](FMassEntityQuery& Query)
	{
		Query.Initialize(EntityManager);
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
		Query.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateDisableObstacleTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);;
		Query.RegisterWithProcessor(*this);
	};

	SetupQuery(BuildObstacleQuery);
	BuildObstacleQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);

	SetupQuery(RepairObstacleQuery);
	RepairObstacleQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::All);

	SetupQuery(PauseObstacleQuery);
	PauseObstacleQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All);

	SetupQuery(IdleObstacleQuery);
	IdleObstacleQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);
}
// --- Main Execution ---

void UDynamicObstacleRegProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun -= ExecutionInterval;

    // --- NEW: Destroy volumes spawned in the previous frame ---
    for (const TWeakObjectPtr<AActor>& VolumePtr : SpawnedNavVolumes)
    {
        if (VolumePtr.IsValid())
        {
            VolumePtr->Destroy();
        }
    }
    SpawnedNavVolumes.Empty(); // Clear the array for the new frame

    UMassNavigationSubsystem* NavSys = Context.GetWorld()->GetSubsystem<UMassNavigationSubsystem>();
    if (!ensure(NavSys != nullptr))
    {
        return;
    }

    // Clear and re-initialize the grid for this frame.
    NavSys->GetObstacleGridMutable().Initialize(InitialCellSize);

    // Pass 1: Collect static obstacles and immediately process dynamic ones.
    auto ProcessQuery = [&](FMassEntityQuery& Query)
    {
        CollectAndProcessObstacles(Context, Query, *NavSys);
    };

    ProcessQuery(BuildObstacleQuery);
    ProcessQuery(RepairObstacleQuery);
    ProcessQuery(PauseObstacleQuery);
    ProcessQuery(IdleObstacleQuery);
}

void UDynamicObstacleRegProcessor::CollectAndProcessObstacles(FMassExecutionContext& Context, FMassEntityQuery& Query, UMassNavigationSubsystem& NavSys)
{
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Context.GetWorld());
    if (!NavSystem) return;

    Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto& Colliders = ChunkContext.GetFragmentView<FMassAvoidanceColliderFragment>();
        const TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (CharList[i].bIsFlying) continue;

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FVector Location = CharList[i].PositionedTransform.GetLocation();
            
            FNavLocation NavLoc;
            if (!NavSystem->ProjectPointToNavigation(Location, NavLoc, FVector(100.f, 100.f, 300.f)))
            {
                continue;
            }

            const float Radius = Colliders[i].GetCircleCollider().Radius;
            
            // Stationary units should always be obstacles
            AddSingleObstacleToGrid(NavSys, Entity, Location, Radius);
        }
    });
}




void UDynamicObstacleRegProcessor::AddSingleObstacleToGrid(UMassNavigationSubsystem& NavSys, const FMassEntityHandle Entity, const FVector& Location, const float Radius)
{
    // ... implementation is unchanged ...
    UWorld* World = NavSys.GetWorld();
    if (Radius > LargeRadiusThreshold)
    {
        const float SubObstacleDiameter = SubObstacleRadius * 2.0f;
        const int32 NumSubObstacles = FMath::CeilToInt((2.0f * PI * Radius) / SubObstacleDiameter);

        for (int32 j = 0; j < NumSubObstacles; ++j)
        {
            const float Angle = (static_cast<float>(j) / NumSubObstacles) * 2.0f * PI;
            const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
            const FBox SubBounds(Location + Offset - FVector(SubObstacleRadius), Location + Offset + FVector(SubObstacleRadius));

            if (Debug) DrawDebugBox(World, SubBounds.GetCenter(), SubBounds.GetExtent(), SubBoxColor_Circle, false, LifeTime, 0, 2.0f);

            NavSys.GetObstacleGridMutable().Add(FMassNavigationObstacleItem(Entity, EMassNavigationObstacleFlags::HasColliderData), SubBounds);
        }
    }
    else
    {
        const FBox Bounds(Location - FVector(Radius), Location + FVector(Radius));
        if (Debug) DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), BoxColor_Single, false, LifeTime, 0, 2.0f);
        NavSys.GetObstacleGridMutable().Add(FMassNavigationObstacleItem(Entity, EMassNavigationObstacleFlags::HasColliderData), Bounds);
    }
}
