#include "Mass/States/AttackStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h" // Für Schadens-Signal

// Fragmente und Tags
#include "MassActorSubsystem.h"   
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassActorSubsystem.h"  
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"


UAttackStateProcessor::UAttackStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UAttackStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UAttackStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None); // Exclude Chase too
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None); // Already excluded by other logic, but explicit

    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Ensure the member SignalSubsystem is valid (initialized in Initialize)
    if (!SignalSubsystem) return;

    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference, DO NOT capture SignalSubsystem directly
        [this, &PendingSignals](FMassExecutionContext& ChunkContext) // Removed SignalSubsystem capture here
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // State modification stays here
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Target Lost ---
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet() && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder); // Adjust UnitSignals::Run based on payload struct
                continue;
            }

            // --- Attack Cycle Timer ---
            StateFrag.StateTimer += ExecutionInterval; // State modification stays here

            if (StateFrag.StateTimer <= Stats.AttackDuration)
            {
                // --- Range Check ---
                const float EffectiveAttackRange = Stats.AttackRange;
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    // --- Melee Impact Check ---
                    if (!Stats.bUseProjectile && !StateFrag.HasAttacked)
                    {
                        // Queue signal instead of sending directly
                        PendingSignals.Emplace(Entity, UnitSignals::MeleeAttack); // Adjust UnitSignals::MeleeAttack
                        StateFrag.HasAttacked = true; // State modification stays here
                        // Note: We queue the signal but modify state immediately. This might
                        // slightly change behavior if the signal was expected to trigger
                        // something *before* HasAttacked was set. Usually okay.
                    }
                    // else if (Stats.bUseProjectile && !StateFrag.HasAttacked) { /* Handle projectile signal queuing */ }
                }
                else // --- Target moved out of range ---
                {
                     // Queue signal instead of sending directly
                    PendingSignals.Emplace(Entity, UnitSignals::Chase); // Adjust UnitSignals::Chase
                    continue;
                }
            }
            else if (!StateFrag.SwitchingState) // --- Attack Duration Over ---
            {
                StateFrag.SwitchingState = true;
                 // Queue signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Pause); // Adjust UnitSignals::Pause
                StateFrag.HasAttacked = false; // State modification stays here
                continue;
            }
        }
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            // Capture the weak subsystem pointer and move the pending signals list
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                // Check if the subsystem is still valid on the Game Thread
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSignalPayload& Payload : SignalsToSend)
                    {
                        // Check Payload validity if needed (e.g., if SignalStruct could be null)
                        if (!Payload.SignalName.IsNone()) // Or check based on your signal type
                        {
                           // Send signal safely from the Game Thread
                           StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                           // Or use the appropriate SignalEntity overload based on your signal type
                           // StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                           // StrongSignalSubsystem->SignalEntity(Payload.SignalID, Payload.TargetEntity);
                        }
                    }
                }
                // else: Subsystem was destroyed before the task could run, signals are lost (usually acceptable)
            });
        }
    }
}