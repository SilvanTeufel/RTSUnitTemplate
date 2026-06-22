// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToBaseStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "MassActorSubsystem.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/UnitBase.h"

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
    // Client prediction: filled on the client to anticipate the server stop (Optional so the
    // server query still matches entities that lack it). Actor is read for MovementAcceptanceRadius.
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional); // crowd-settle: detect blocked workers
    // NO FGoToBaseTargetFragment - info moved to WorkerStats

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::All);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    
    
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
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
        const bool bHasVelocity = VelocityList.Num() > 0;

        const int32 NumEntities = ChunkContext.GetNumEntities();
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            // Increment state timer
            AIState.StateTimer += ExecutionInterval;
            
            if (WorkerStats.BuildingAreaAvailable && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBuild, Entity);
                }
                continue;
            }

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
            const float DistanceToTargetCenter = FVector::Dist2D(CurrentTransform.GetLocation(), WorkerStats.BasePosition);
 
            MoveTarget.DistanceToGoal = DistanceToTargetCenter - WorkerStats.BaseArrivalDistance; // Update distance
            if (DistanceToTargetCenter <= WorkerStats.BaseArrivalDistance && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                // Stop movement immediately and mirror to all clients
                StopMovement(MoveTarget, World);
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::ReachedBase, Entity);
                }
                continue;
            }

            // --- 1b. Crowd settle ---
            // Outer workers in a pile-up can never reach the tight arrival ring. If we are already
            // near the base but effectively blocked (not moving) after a moment in GoToBase, settle
            // here via the same ReachedBase path so we deposit + go Idle instead of jittering forever.
            if (bHasVelocity && !AIState.SwitchingState &&
                AIState.StateTimer >= CrowdSettleMinStateTime &&
                DistanceToTargetCenter <= WorkerStats.BaseArrivalDistance * CrowdSettleRadiusMultiplier)
            {
                const float Speed2D = VelocityList[i].Value.Size2D();
                if (Speed2D <= CrowdSettleSpeedThreshold)
                {
                    AIState.SwitchingState = true;
                    StopMovement(MoveTarget, World);
                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::ReachedBase, Entity);
                    }
                    continue;
                }
            }

            //if (!AIState.SwitchingState)
                //UpdateMoveTarget(MoveTarget, WorkerStats.BasePosition, CombatStatsList[i].RunSpeed, World);
            

            // --- 2. Movement Logic ---
        } // End loop through entities
    }); // End ForEachEntityChunk
}

// CLIENT: prediction only. The previous implementation authored the next state's tag here off the
// un-replicated WorkerStats + PlaceholderSignal. WorkerStats.BaseAvailable defaults to false on the
// client, so that path fired immediately and -- combined with the bubble re-applying tags without
// hysteresis -- produced the documented per-cycle GetUnitState flip / broken animation. The bubble
// is now the sole tag authority for workers; here we only predict movement toward the replicated
// MoveTarget.Center so the client anticipates the server stop instead of overshooting the base.
void UGoToBaseStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        if (PredList.Num() == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FVector CurrentLocation = TransformList[i].GetTransform().GetLocation();
            const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            float ArrivalDistance = ArrivalDistanceMultiplier * 50.f; // fallback if actor not resolved yet
            if (ActorList.Num() > 0)
            {
                if (const AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].Get()))
                {
                    ArrivalDistance = ArrivalDistanceMultiplier * Unit->MovementAcceptanceRadius;
                }
            }

            PredictWorkerStop(PredList[i], CurrentLocation, MoveTarget.Center, MoveTarget.DesiredSpeed.Get(), ArrivalDistance);
        }
    });
}
