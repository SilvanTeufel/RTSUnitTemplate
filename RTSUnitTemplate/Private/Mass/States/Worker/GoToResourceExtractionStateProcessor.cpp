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
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // Might read velocity to see if stuck? Optional.

    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

    UE_LOG(LogTemp, Warning, TEXT("!!!!UGoToResourceExtractionStateProcessor!!!"));
    QUICK_SCOPE_CYCLE_COUNTER(STAT_UGoToResourceExtractionStateProcessor_Execute);

    UWorld* World = Context.GetWorld(); // Get World via Context
    if (!World) return;

    UMassSignalSubsystem* LocalSignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
    if (!LocalSignalSubsystem) return;

    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = LocalSignalSubsystem;
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture World for helper functions
        [&PendingSignals, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto CombatStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        UE_LOG(LogTemp, Warning, TEXT("UGoToResourceExtractionStateProcessor NumEntities: %d"), NumEntities);
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAITargetFragment& AiTargetFrag = TargetList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& CombatStatsFrag = CombatStatsList[i];
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            // --- 1. Check Target Validity ---
            if (!AiTargetFrag.bHasValidTarget || !AiTargetFrag.TargetEntity.IsSet())
            {
                // Target is lost or invalid. Signal to go idle or find a new task.
                PendingSignals.Emplace(Entity, UnitSignals::Idle); // Use appropriate signal
                StopMovement(MoveTarget, World); // Stop current movement
                continue;
            }

            // --- 2. Distance Check for Arrival ---
            const float CurrentLocationZ = Transform.GetLocation().Z; // Preserve Z for comparison/movement
            const FVector TargetLocation = FVector(AiTargetFrag.LastKnownLocation.X, AiTargetFrag.LastKnownLocation.Y, CurrentLocationZ); // Move towards target XY at current Z
            const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetLocation);
            //const float ArrivalDistSq = FMath::Square(WorkerStatsFrag.ResourceArrivalDistance);

            // --- 3. Handle Arrival ---
            /*
            if (DistSq <= ArrivalDistSq)
            {
                // Arrived at the resource node area.
                // UE_LOG(LogTemp, Log, TEXT("Entity %d: Arrived at resource extraction area. Queuing Signal %s."), Entity.Index, *UnitSignals::HandleResourceExtractionArea.ToString());

                // Queue the signal for the next phase (actual extraction)
                PendingSignals.Emplace(Entity, UnitSignals::ResourceExtraction);

                // Stop movement now that we've arrived.
                StopMovement(MoveTarget, World);

                continue; // Move to next entity
            }*/

            // --- 4. Update Movement (If Not Arrived) ---
            // Continue moving towards the target resource node location.
            UpdateMoveTarget(MoveTarget, TargetLocation, CombatStatsFrag.RunSpeed, World);
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
                        StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                    }
                }
            }
        });
    }
}