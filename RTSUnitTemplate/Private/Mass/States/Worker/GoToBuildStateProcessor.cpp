// Fill out your copyright notice in the Description page of Project Settings.

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
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

// No Actor/Component includes needed in this file anymore


UGoToBuildStateProcessor::UGoToBuildStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToBuildStateProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // NO FMassActorFragment
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly); // Contains target pos/radius now
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);       // For speed

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::All);

    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaSeconds = Context.GetDeltaTimeSeconds();
    UWorld* World = EntityManager.GetWorld();

    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("UGoToBuildStateProcessor: Cannot execute without a valid UWorld."));
        return;
    }

    // Use the engine/Mass provided FMassSignalPayload struct
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, DeltaSeconds, World, &PendingSignals](FMassExecutionContext& Context)
    {
        // --- Get Fragment Views ---
        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetFragmentView<FMassWorkerStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = Context.GetFragmentView<FMassCombatStatsFragment>();

        const int32 NumEntities = Context.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];

            // Increment state timer
            AIState.StateTimer += DeltaSeconds;

            // --- Pre-checks REMOVED (Validation assumed external) ---
            // No AUnitBase or ABuildArea access here.

            // --- 1. Arrival Check ---
            //const float BuildAreaArrivalDistance = WorkerStats.BuildAreaArrivalDistance;
            // Get target info from WorkerStats fragment
            const FVector BuildAreaPosition = WorkerStats.BuildAreaPosition;
            //const float BuildAreaRadius = WorkerStats.BuildAreaRadius;

            // Basic validation of data from fragment (more robust checks should be external)
            if (BuildAreaPosition.IsNearlyZero()) // Example basic check
            {
                 UE_LOG(LogTemp, Warning, TEXT("Entity %d: GoToBuildStateProcessor: Invalid BuildAreaPosition in WorkerStats. Queuing Idle signal."), Entity.Index);
                 // Use FMassSignalPayload constructor
                 PendingSignals.Emplace(Entity, UnitSignals::Idle); // Use appropriate fallback signal FName
                 StopMovement(MoveTarget, World);
                 continue;
            }

            const float DistanceToTargetCenter = FVector::Dist(CurrentTransform.GetLocation(), BuildAreaPosition);
            //const float DistanceToTargetEdge = DistanceToTargetCenter - BuildAreaRadius;

            MoveTarget.DistanceToGoal = DistanceToTargetCenter; // Update distance

            /*
            if (DistanceToTargetEdge <= BuildAreaArrivalDistance)
            {
                UE_LOG(LogTemp, Log, TEXT("Entity %d: GoToBuildStateProcessor: Arrived at target location. Queuing signal '%s'."), Entity.Index, *UnitSignals::Build.ToString());
                // Use FMassSignalPayload constructor
                PendingSignals.Emplace(Entity, UnitSignals::Build);
                StopMovement(MoveTarget, World);
                continue;
            }
            */

            // --- 2. Movement Logic ---
            const float TargetSpeed = Stats.RunSpeed;
            // Use the externally provided helper function
            UpdateMoveTarget(MoveTarget, BuildAreaPosition, TargetSpeed, World);

        } // End loop through entities
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task to Send Queued Signals (Same as before) ---
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
                    for (const FMassSignalPayload& Payload : SignalsToSend) // Iterate FMassSignalPayload
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