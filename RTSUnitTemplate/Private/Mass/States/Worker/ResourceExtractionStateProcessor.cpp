// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
   // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);     // Write Velocity to stop movement
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    // Ensure all entities processed by this will have these fragments.
    // Consider adding EMassFragmentPresence::All checks if necessary,
    // though processors usually run *after* state setup ensures fragments exist.

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.RegisterWithProcessor(*this);
}

void UResourceExtractionStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
   // QUICK_SCOPE_CYCLE_COUNTER(STAT_UResourceExtractionStateProcessor_Execute);
    
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    if (!SignalSubsystem) return;
    
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(EntityQuery.GetNumMatchingEntities(EntityManager)); // Optional pre-allocation

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this,  &PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        //const auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

            //UE_LOG(LogTemp, Log, TEXT("UResourceExtractionStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];
          //  FMassVelocityFragment& Velocity = VelocityList[i];
    ;

            // --- 2. Stop Movement ---
            // Ensure the worker isn't moving while extracting
           // Velocity.Value = FVector::ZeroVector;
       
            if (!WorkerStatsFrag.ResourceAvailable && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                // Target is lost or invalid. Signal to go idle or find a new task.
                PendingSignals.Emplace(Entity, UnitSignals::GoToBase); // Use appropriate signal
                //StopMovement(MoveTarget, World); // Stop current movement
                continue;
            }
            
            // --- 3. Increment Extraction Timer ---
            StateFrag.StateTimer += ExecutionInterval;
            PendingSignals.Emplace(Entity, UnitSignals::SyncCastTime);
            // --- 4. Check if Extraction Time Elapsed ---
           
            if (StateFrag.StateTimer >= WorkerStatsFrag.ResourceExtractionTime && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                //StateFrag.StateTimer = 0.f;
                PendingSignals.Emplace(Entity, UnitSignals::GetResource); // Use your actual signal name
                continue; // Move to next entity
            }
            // --- Still Extracting ---
            // Keep velocity zeroed (already done above) and let the timer continue.
        }
            
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
                    }
                }
            });
        }
    }
}
