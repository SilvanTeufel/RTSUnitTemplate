// Fill out your copyright notice in the Description page of Project Settings.

#include "Mass/States/Worker/GoToResourceExtractionStateProcessor.h" // Header for this processor
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"     // For FTransformFragment
#include "MassMovementFragments.h"   // For FMassMoveTargetFragment, FMassVelocityFragment (optional but good practice)
#include "MassSignalSubsystem.h"
#include "Async/Async.h"

// --- Include your specific project Fragments, Tags, and Signals ---
#include "Mass/UnitMassTag.h"       // Contains State Tags (FMassStateGoToResourceExtractionTag)
#include "Mass/Signals/MySignals.h"


UGoToResourceExtractionStateProcessor::UGoToResourceExtractionStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToResourceExtractionStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::All);

    // Fragments needed:
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);       // Read TargetEntity, LastKnownLocation, bHasValidTarget
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);         // Read current location for distance check
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);    // Read RunSpeed
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);   // Read ResourceArrivalDistance
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);    // Write new move target / stop movement
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // Might read velocity to see if stuck? Optional.

    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToResourceExtractionStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_UGoToResourceExtractionStateProcessor_Execute);

    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    UWorld* World = Context.GetWorld(); // Get World via Context
    if (!World) return;

    if (!SignalSubsystem) return;


    
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture World for helper functions
        [this, &PendingSignals, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto CombatStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAITargetFragment& AiTargetFrag = TargetList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& CombatStatsFrag = CombatStatsList[i];
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            AIState.StateTimer += ExecutionInterval;
            // --- 1. Check Target Validity ---
            if (!WorkerStatsFrag.ResourceAvailable)
            {
                // Target is lost or invalid. Signal to go idle or find a new task.
                PendingSignals.Emplace(Entity, UnitSignals::GoToBase); // Use appropriate signal
                StopMovement(MoveTarget, World); // Stop current movement
                continue;
            }

            const float DistanceToTargetCenter = FVector::Dist(Transform.GetLocation(), WorkerStatsFrag.ResourcePosition);
            
            if (DistanceToTargetCenter <= WorkerStatsFrag.ResourceArrivalDistance && AIState.StateTimer >= ExecutionInterval*3.f)
            {
                AIState.StateTimer = 0.f;
                // Queue signal for reaching the base
                PendingSignals.Emplace(Entity, UnitSignals::ResourceExtraction); // Use appropriate signal name
                StopMovement(MoveTarget, World);
                continue;
            }
            // --- 4. Update Movement (If Not Arrived) ---
            // Continue moving towards the target resource node location.
            UpdateMoveTarget(MoveTarget, WorkerStatsFrag.ResourcePosition, CombatStatsFrag.RunSpeed, World);
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
