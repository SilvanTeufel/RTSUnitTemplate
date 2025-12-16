// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToBaseStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/UnitMassTag.h"

// No Actor includes needed

UGoToBaseStateProcessor::UGoToBaseStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToBaseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // NO FMassActorFragment
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // To update movement
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly); // Get BaseArrivalDistance, BasePosition, BaseRadius
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Get RunSpeed
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly); 
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);    // Update StateTimer
    // NO FGoToBaseTargetFragment - info moved to WorkerStats

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::All);


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBaseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToBaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun -= ExecutionInterval;

    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UGoToBaseStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        // --- Get Fragment Views ---
        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const TConstArrayView<FMassCombatStatsFragment> CombatStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        const int32 NumEntities = ChunkContext.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            // Increment state timer
            AIState.StateTimer += ExecutionInterval;
            
            if (!WorkerStats.BaseAvailable)
            {
                AIState.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                }
                continue;
            }

            // --- 1. Arrival Check ---
            const float DistanceToTargetCenter = FVector::Dist(CurrentTransform.GetLocation(), WorkerStats.BasePosition) - CharFrag.CapsuleRadius;

            if (DistanceToTargetCenter <= WorkerStats.BaseArrivalDistance && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                // Stop movement immediately and mirror to all clients
                FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                StopMovement(MoveTarget, World);
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::ReachedBase, Entity);
                }
                continue;
            }

            // --- 2. Movement Logic ---
            // Use the externally provided helper function
        } // End loop through entities
    }); // End ForEachEntityChunk
}

void UGoToBaseStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // If base not available, switch to Idle locally
            if (!WorkerStats.BaseAvailable)
            {
                StateFrag.SwitchingState = true;
                auto& Defer = ChunkContext.Defer();
                Defer.RemoveTag<FMassStateRunTag>(Entity);
                Defer.RemoveTag<FMassStateChaseTag>(Entity);
                Defer.RemoveTag<FMassStateAttackTag>(Entity);
                Defer.RemoveTag<FMassStatePauseTag>(Entity);
                Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
                Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
                Defer.RemoveTag<FMassStateCastingTag>(Entity);
                Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
                Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
                Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
                Defer.RemoveTag<FMassStateBuildTag>(Entity);
                Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
                Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
                Defer.AddTag<FMassStateIdleTag>(Entity);
                continue;
            }

            // Arrival check using GoToBase conditions
            const float DistanceToTargetCenter = FVector::Dist(CurrentTransform.GetLocation(), WorkerStats.BasePosition) - CharFrag.CapsuleRadius;
            if (DistanceToTargetCenter <= WorkerStats.BaseArrivalDistance && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                auto& Defer = ChunkContext.Defer();
                Defer.RemoveTag<FMassStateRunTag>(Entity);
                Defer.RemoveTag<FMassStateChaseTag>(Entity);
                Defer.RemoveTag<FMassStateAttackTag>(Entity);
                Defer.RemoveTag<FMassStatePauseTag>(Entity);
                Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
                Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
                Defer.RemoveTag<FMassStateCastingTag>(Entity);
                Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
                Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
                Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
                Defer.RemoveTag<FMassStateBuildTag>(Entity);
                Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
                Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
                Defer.AddTag<FMassStateIdleTag>(Entity);
                continue;
            }
        }
    });
}
