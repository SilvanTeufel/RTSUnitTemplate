#include "Mass/States/ChaseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationTypes.h"

#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"

#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"


UChaseStateProcessor::UChaseStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UChaseStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::All); // Nur Chase-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

    EntityQuery.RegisterWithProcessor(*this);
}

void UChaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Get World and Signal Subsystem once
    UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    UMassSignalSubsystem* LocalSignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!LocalSignalSubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("UChaseStateProcessor: Could not get SignalSubsystem!"));
        return;
    }
    // Make a weak pointer copy for safe capture in the async task
    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = LocalSignalSubsystem;


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference. Capture World for helper functions.
        // Do NOT capture LocalSignalSubsystem directly here.
        [&PendingSignals, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& Stats = StatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for Update/Stop

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Target Lost ---
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                // Queue signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder);
                // Potentially clear MoveTarget here too if needed when losing target
                // StopMovement(MoveTarget, World); // Or maybe UpdateMoveTarget to a default spot?
                continue;
            }

            // --- Distance Check ---
            const float EffectiveAttackRange = Stats.AttackRange;
            const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
            const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

            // --- In Attack Range ---
            if (DistSq <= AttackRangeSq)
            {
                // Queue signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Pause);

                // StopMovement modifies fragment directly, keep it here
                StopMovement(MoveTarget, World);
                continue;
            }

            // --- Still Chasing (Out of Range) ---
            // UpdateMoveTarget modifies fragment directly, keep it here
            UpdateMoveTarget(MoveTarget, TargetFrag.LastKnownLocation, Stats.RunSpeed, World);
            // StateFrag.StateTimer = 0.f; // Reset timer if Chase has one?
        }
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        // Capture the weak subsystem pointer and move the pending signals list
        AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
        {
            // Check if the subsystem is still valid on the Game Thread
            if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
            {
                for (const FMassSignalPayload& Payload : SignalsToSend)
                {
                    // Check if the FName is valid before sending
                    if (!Payload.SignalName.IsNone())
                    {
                       // Send signal safely from the Game Thread using FName
                       StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                    }
                }
            }
        });
    }
}