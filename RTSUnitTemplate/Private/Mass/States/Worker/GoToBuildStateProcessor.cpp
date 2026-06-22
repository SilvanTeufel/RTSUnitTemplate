// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToBuildStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"     // For FMassSignalPayload and sending signals
#include "Async/Async.h"
#include "Engine/World.h"

// Your project specific includes
#include "MassNavigationFragments.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"


UGoToBuildStateProcessor::UGoToBuildStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToBuildStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // NO FMassActorFragment
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly); // Contains target pos/radius now
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);       // For speed
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // Client prediction: filled on the client to anticipate the server stop (Optional so the
    // server query still matches entities that lack it). Actor is read for MovementAcceptanceRadius.
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::All);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
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

void UGoToBuildStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

// CLIENT: prediction only. Never authors state tags (the bubble is the sole tag authority for
// workers) and never writes the authoritative MoveTarget. WorkerStats is NOT replicated, so the
// destination comes from the replicated MoveTarget.Center and the arrival distance is recomputed
// from the replicated MovementAcceptanceRadius.
void UGoToBuildStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        if (PredList.Num() == 0) return; // no prediction fragment on this archetype -> nothing to do

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

void UGoToBuildStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();

    if (!World)
    {
        return;
    }

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World](FMassExecutionContext& Context)
    {
        // --- Get Fragment Views ---
        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetFragmentView<FMassWorkerStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = Context.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
 
        const int32 NumEntities = Context.GetNumEntities();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            // Increment state timer
            AIState.StateTimer += ExecutionInterval;
            
            // Basic validation of data from fragment (more robust checks should be external)
            if ((WorkerStats.BuildingAvailable || !WorkerStats.BuildingAreaAvailable) && !AIState.SwitchingState) // Example basic check
            {
                 if (SignalSubsystem)
                 {
                     SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToBase, Entity);
                 }
                 continue;
            }
            
            const float DistanceToTargetCenter = FVector::Dist2D(CurrentTransform.GetLocation(), WorkerStats.BuildAreaPosition);

            MoveTarget.DistanceToGoal = DistanceToTargetCenter - WorkerStats.BuildAreaArrivalDistance; // Update distance
            if (DistanceToTargetCenter <= WorkerStats.BuildAreaArrivalDistance && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                // Stop movement immediately and mirror to all clients
                StopMovement(MoveTarget, World);
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Build, Entity);
                }
                continue;
            }
        } // End loop through entities
    }); // End ForEachEntityChunk

}
