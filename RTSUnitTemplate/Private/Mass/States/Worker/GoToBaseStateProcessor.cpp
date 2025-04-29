// Fill out your copyright notice in the Description page of Project Settings.

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

UGoToBaseStateProcessor::UGoToBaseStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToBaseStateProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // NO FMassActorFragment
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // To update movement
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly); // Get BaseArrivalDistance, BasePosition, BaseRadius
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Get RunSpeed
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);    // Update StateTimer
    // NO FGoToBaseTargetFragment - info moved to WorkerStats

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::All);

    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaSeconds = Context.GetDeltaTimeSeconds();
    UWorld* World = EntityManager.GetWorld();

    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("UGoToBaseStateProcessor: Cannot execute without a valid UWorld."));
        return;
    }

    // Use FMassSignalPayload
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, DeltaSeconds, World, &PendingSignals](FMassExecutionContext& Context)
    {
        // --- Get Fragment Views ---
        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetFragmentView<FMassWorkerStatsFragment>();
        const TConstArrayView<FMassCombatStatsFragment> CombatStatsList = Context.GetFragmentView<FMassCombatStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();

        const int32 NumEntities = Context.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            const FMassCombatStatsFragment& CombatStats = CombatStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];

            // Increment state timer
            AIState.StateTimer += DeltaSeconds;

            // --- Pre-checks (Fragment Data Validation) ---
            // Get target info from WorkerStats fragment
            const FVector TargetPosition = WorkerStats.BasePosition;
            const float TargetRadius = WorkerStats.BaseRadius;

            // Basic validation: Ensure target position was set (more robust checks assumed external)
            if (TargetPosition.IsNearlyZero())
            {
                 UE_LOG(LogTemp, Warning, TEXT("Entity %d: GoToBaseStateProcessor: Invalid BasePosition in WorkerStats. Queuing Idle signal."), Entity.Index);
                 PendingSignals.Emplace(Entity, UnitSignals::Idle); // Use appropriate fallback signal FName
                 StopMovement(MoveTarget, World);
                 continue;
            }

            // --- 1. Arrival Check ---
            const float BaseArrivalDistance = WorkerStats.BaseArrivalDistance; // Get from Worker stats
            const float DistanceToTargetCenter = FVector::Dist(CurrentTransform.GetLocation(), TargetPosition);
            const float DistanceToTargetEdge = DistanceToTargetCenter - TargetRadius;

            MoveTarget.DistanceToGoal = DistanceToTargetCenter; // Update distance in move target

            if (DistanceToTargetEdge <= BaseArrivalDistance)
            {
                UE_LOG(LogTemp, Log, TEXT("Entity %d: GoToBaseStateProcessor: Arrived at Base location. Queuing signal '%s'."), Entity.Index, *UnitSignals::ReachedBase.ToString());
                // Queue signal for reaching the base
                PendingSignals.Emplace(Entity, UnitSignals::ReachedBase); // Use appropriate signal name
                StopMovement(MoveTarget, World);
                continue;
            }

            // --- 2. Movement Logic ---
            const float TargetSpeed = CombatStats.RunSpeed; // Get speed from Combat stats
            // Use the externally provided helper function
            UpdateMoveTarget(MoveTarget, TargetPosition, TargetSpeed, World);

        } // End loop through entities
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
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