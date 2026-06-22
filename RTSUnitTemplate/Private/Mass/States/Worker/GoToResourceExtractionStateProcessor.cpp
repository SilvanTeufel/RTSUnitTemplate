// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToResourceExtractionStateProcessor.h" // Header for this processor
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"     // For FTransformFragment
#include "MassMovementFragments.h"   // For FMassMoveTargetFragment, FMassVelocityFragment (optional but good practice)
#include "MassSignalSubsystem.h"
#include "Async/Async.h"

// --- Include your specific project Fragments, Tags, and Signals ---
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"       // Contains State Tags (FMassStateGoToResourceExtractionTag)
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"


UGoToResourceExtractionStateProcessor::UGoToResourceExtractionStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToResourceExtractionStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::All);

    // Fragments needed:
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);       // Read TargetEntity, LastKnownLocation, bHasValidTarget
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);         // Read current location for distance check
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);    // Read RunSpeed
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);   // Read ResourceArrivalDistance
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);    // Write new move target / stop movement
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // Client prediction: filled on the client to anticipate the server stop (Optional so the
    // server query still matches entities that lack it). Actor is read for MovementAcceptanceRadius.
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // Might read velocity to see if stuck? Optional.


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToResourceExtractionStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //QUICK_SCOPE_CYCLE_COUNTER(STAT_UGoToResourceExtractionStateProcessor_Execute);
    //TRACE_CPUPROFILER_EVENT_SCOPE(UGoToResourceExtractionStateProcessor_Execute);
    
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

// CLIENT: prediction only. The previous build re-authored worker state tags here (and self-
// signalled), which fought the bubble's tag re-application without hysteresis and caused the
// documented per-cycle GetUnitState flip / broken animation. That is gone: the bubble is the sole
// tag authority for workers. ResourceAvailable / BuildingAreaAvailable live in the un-replicated
// WorkerStats and are meaningless on the client, so we ignore them and only predict movement
// toward the replicated MoveTarget.Center.
void UGoToResourceExtractionStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

void UGoToResourceExtractionStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld(); // Get World via Context
    if (!World) return;

    if (!SignalSubsystem) return;

    // Using deferred signal commands via Context, no manual arrays or AsyncTask dispatch needed

    EntityQuery.ForEachEntityChunk(Context,
        // Capture World for helper functions
        [this, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const auto CombatStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& AIState = AIStateList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FMassCombatStatsFragment& CombatStats = CombatStatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            AIState.StateTimer += ExecutionInterval;

            // --- 1. Check Target Validity ---

            if (!WorkerStatsFrag.ResourceAvailable)
            {
                // Target is lost or invalid. Signal to go idle or find a new task.
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBase, Entity);
                }
                continue;
            }

            if (WorkerStatsFrag.BuildingAreaAvailable && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;

                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBuild, Entity);
                }
                continue;
            }

            const float DistanceToTargetCenter = FVector::Dist2D(Transform.GetLocation(), WorkerStatsFrag.ResourcePosition);

            MoveTarget.DistanceToGoal = DistanceToTargetCenter - WorkerStatsFrag.ResourceArrivalDistance; // Update distance

            if (DistanceToTargetCenter <= WorkerStatsFrag.ResourceArrivalDistance && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;

                // Stop movement and mirror to clients when reaching the resource
                StopMovement(MoveTarget, World);
                // Queue signals thread-safely using the deferred command buffer
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::ResourceExtraction, Entity);
                }
                continue;
            }

           // if (!AIState.SwitchingState)
                //UpdateMoveTarget(MoveTarget, WorkerStatsFrag.ResourcePosition, CombatStats.RunSpeed, World);
        }
    }); // End ForEachEntityChunk


}
