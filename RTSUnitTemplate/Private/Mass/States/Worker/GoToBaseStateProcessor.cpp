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

// No Actor includes needed

UGoToBaseStateProcessor::UGoToBaseStateProcessor(): EntityQuery()
{
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
    
    UWorld* World = EntityManager.GetWorld();

    if (!World)
    {
        //UE_LOG(LogTemp, Error, TEXT("UGoToBaseStateProcessor: Cannot execute without a valid UWorld."));
        return;
    }

    if (!SignalSubsystem) return;
    // Use FMassSignalPayload
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(Context,
        [this, &PendingSignals](FMassExecutionContext& Context)
    {
        // --- Get Fragment Views ---
        
        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetFragmentView<FMassWorkerStatsFragment>();
        const TConstArrayView<FMassCombatStatsFragment> CombatStatsList = Context.GetFragmentView<FMassCombatStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
      
        const int32 NumEntities = Context.GetNumEntities();

            //UE_LOG(LogTemp, Log, TEXT("UGoToBaseStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];

            // Increment state timer
            AIState.StateTimer += ExecutionInterval;
            
            if (!WorkerStats.BaseAvailable && AIState.StateTimer >= 5.f && !AIState.SwitchingState)
            {
                 AIState.SwitchingState = true;
                 PendingSignals.Emplace(Entity, UnitSignals::Idle);
                 continue;
            }

            // --- 1. Arrival Check ---
            const float DistanceToTargetCenter = FVector::Dist(CurrentTransform.GetLocation(), WorkerStats.BasePosition);

            if (DistanceToTargetCenter <= WorkerStats.BaseArrivalDistance) // && !AIState.SwitchingState
            {
                AIState.SwitchingState = true;
                // Queue signal for reaching the base
                PendingSignals.Emplace(Entity, UnitSignals::ReachedBase); // Use appropriate signal name
                continue;
            }
            

            // --- 2. Movement Logic ---
            // Use the externally provided helper function
        } // End loop through entities
            
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSignalPayload& Payload : SignalsToSend)
                    {
                        if (!Payload.SignalName.IsNone())
                        {
                           StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                        }
                         else { /* Log error */ }
                    }
                }
                else { /* Log error: Subsystem invalid */ }
            });
        }
        else { /* Log error: Subsystem not found */ }
    }
}
