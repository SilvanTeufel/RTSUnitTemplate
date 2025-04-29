// Fill out your copyright notice in the Description page of Project Settings.

#include "Mass/States/Worker/ResourceExtractionStateProcessor.h" // Header for this processor
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"     // For FTransformFragment (if needed later)
#include "MassMovementFragments.h"   // For FMassVelocityFragment
#include "MassSignalSubsystem.h"
#include "Async/Async.h"            // Required for AsyncTask

// --- Include your specific project Fragments, Tags, and Signals ---
#include "Mass/UnitMassTag.h"       // Contains State Tags (FMassStateResourceExtractionTag) and potentially FMassSignalPayload definition
#include "Mass/Signals/MySignals.h"

// Make sure UnitSignals::GoToBase and UnitSignals::Idle (or your equivalents) are defined and accessible

// Ensure FMassSignalPayload is defined (it was in your UnitFragments.h snippet)
// struct FMassSignalPayload { ... }; // No need to redefine if included via UnitFragments.h


UResourceExtractionStateProcessor::UResourceExtractionStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UResourceExtractionStateProcessor::ConfigureQueries()
{
    // Query for entities that are in the Resource Extraction state
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::All);

    // Fragments needed for the logic:
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);     // Read/Write StateTimer
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);   // Read ResourceExtractionTime
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);      // Read target validity (bHasValidTarget) and target entity handle if needed
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);     // Write Velocity to stop movement

    // Ensure all entities processed by this will have these fragments.
    // Consider adding EMassFragmentPresence::All checks if necessary,
    // though processors usually run *after* state setup ensures fragments exist.

    EntityQuery.RegisterWithProcessor(*this);
}

void UResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_UResourceExtractionStateProcessor_Execute);

    UWorld* World = EntityManager.GetWorld(); // Get World via EntityManager
    if (!World) return;

    UMassSignalSubsystem* LocalSignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>(); // Get Subsystem via Context
    if (!LocalSignalSubsystem) return;

    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = LocalSignalSubsystem;
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(EntityQuery.GetNumMatchingEntities(EntityManager)); // Optional pre-allocation

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const auto AiTargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>(); // Using FMassAITargetFragment
        const auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds(); // Get DeltaTime from Context

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];
            const FMassAITargetFragment& AiTargetFrag = AiTargetList[i]; // Get the AI Target fragment
            FMassVelocityFragment& Velocity = VelocityList[i];

            // --- 1. Check if the resource target is still valid (using FMassAITargetFragment) ---
            if (!AiTargetFrag.bHasValidTarget) // Check the flag from the AI Target fragment
            {
                 // Target lost or became invalid while extracting.
                 // Queue signal to go Idle or find a new task/resource node.
                 PendingSignals.Emplace(Entity, UnitSignals::Idle); // Use your actual signal for this case
                 StateFrag.StateTimer = 0.f; // Reset timer as the task failed/stopped
                 Velocity.Value = FVector::ZeroVector; // Ensure velocity is zeroed if we exit early
                 continue; // Move to next entity
            }

            // --- 2. Stop Movement ---
            // Ensure the worker isn't moving while extracting
            Velocity.Value = FVector::ZeroVector;

            // --- 3. Increment Extraction Timer ---
            StateFrag.StateTimer += DeltaTime;

            // --- 4. Check if Extraction Time Elapsed ---
            if (StateFrag.StateTimer >= WorkerStatsFrag.ResourceExtractionTime)
            {
                // Extraction complete!
                // UE_LOG(LogTemp, Log, TEXT("Entity %d: Resource extraction complete. Queuing Signal %s."), Entity.Index, *UnitSignals::GoToBase.ToString());

                // Queue signal to transition to the next state (e.g., returning to base)
                // The handler for this signal should manage spawning the carried resource.
                PendingSignals.Emplace(Entity, UnitSignals::GoToBase); // Use your actual signal name

                // Reset the timer. The new state's processor might use it differently.
                StateFrag.StateTimer = 0.f;

                // NOTE: Spawning the resource is NOT done here. It's deferred to the
                // signal handler or the processor for the 'GoToBase' state.

                continue; // Move to next entity
            }

            // --- Still Extracting ---
            // Keep velocity zeroed (already done above) and let the timer continue.
        }
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
        {
            if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
            {
                for (const FMassSignalPayload& Payload : SignalsToSend)
                {
                    if (!Payload.SignalName.IsNone())
                    {
                        // UE_LOG(LogTemp, Verbose, TEXT("Signaling Entity %d:%d with %s from ResourceExtractionProcessor"), Payload.TargetEntity.Index, Payload.TargetEntity.SerialNumber, *Payload.SignalName.ToString());
                        StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                    }
                }
            }
        });
    }
}