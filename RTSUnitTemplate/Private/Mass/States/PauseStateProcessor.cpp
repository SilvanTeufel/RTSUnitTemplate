#include "Mass/States/PauseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"

UPauseStateProcessor::UPauseStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UPauseStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All); // Nur Pause-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben, Zustand ändern
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Stats lesen (AttackPauseDuration)
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position für Distanzcheck
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UPauseStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UPauseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Get World and Signal Subsystem once
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager to get World
    if (!World) return;

    if (!SignalSubsystem) return;
    // Make a weak pointer copy for safe capture in the async task


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference. Capture World for UpdateMoveTarget.
        // Do NOT capture LocalSignalSubsystem directly here.
        [this, &PendingSignals, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // State modification stays here
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for UpdateMoveTarget

            // --- Target Lost ---
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet() && !StateFrag.SwitchingState)
            {
                // Queue signal instead of sending directly
                StateFrag.SwitchingState = true;
                UE_LOG(LogTemp, Log, TEXT("PauseSWITCH TO PLACEHOLDER!"));
                PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder);
                StopMovement(MoveTarget, World);
                // UpdateMoveTarget stays here as it modifies fragment data directly
                //UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, Stats.RunSpeed, World);
                continue;
            }

            // --- Pause Timer ---
            StateFrag.StateTimer += ExecutionInterval; // State modification stays here
                // --- Pause Over, Decide Next State ---
            const float Dist = FVector::Dist(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                
                if (Dist <= Stats.AttackRange) // --- In Range ---
                {
                    if (StateFrag.StateTimer >= Stats.PauseDuration  && !StateFrag.SwitchingState)
                    {
                        StateFrag.SwitchingState = true;
                        // Check for ranged attack *before* general attack signal if applicable
                        if (Stats.bUseProjectile)
                        {
                            UE_LOG(LogTemp, Log, TEXT("Fire Projectile!"));
                             // Queue signal instead of sending directly
                            PendingSignals.Emplace(Entity, UnitSignals::RangedAttack);
                            // Note: If RangedAttack implies Attack state, maybe only send one?
                            // If not, it will queue both RangedAttack and Attack signals below.
                        }else
                        {
                            PendingSignals.Emplace(Entity, UnitSignals::Attack);
                        }
                    }
                }
                else if (!StateFrag.SwitchingState)
                {
                    StateFrag.SwitchingState = true;
                     // Queue signal instead of sending directly
                    PendingSignals.Emplace(Entity, UnitSignals::Chase);
                }
                // Reset timer or other state if needed upon leaving Pause
                // StateFrag.StateTimer = 0.0f; // Example reset
                continue; // Zustand gewechselt
            // --- Still Paused ---
            // Velocity.Value = FVector::ZeroVector; // If needed, keep here
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
}
