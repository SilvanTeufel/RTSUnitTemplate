// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/IdleStateProcessor.h" // Dein Prozessor-Header

// Andere notwendige Includes...
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"      // FMassVelocityFragment
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

// ...

UIdleStateProcessor::UIdleStateProcessor(): EntityQuery()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UIdleStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);

    // Benötigte Fragmente:
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly);
    // === KORREKTUR HIER ===
    // Timer ist jetzt Teil von FMassAIStateFragment
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // ReadWrite für State und Timer

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UIdleStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UIdleStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // --- Get World and Signal Subsystem once ---
    const UWorld* World = EntityManager.GetWorld(); // Use EntityManager consistently
    if (!World) return;

    if (!SignalSubsystem) return;

    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(Context,
        // Capture PendingSignals by reference. Capture World & EntityManager if needed by inner logic.
        // Do NOT capture LocalSignalSubsystem directly here.
        [this, &PendingSignals, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassPatrolFragment& PatrolFrag = PatrolList[i];

            // --- Check for Valid Target ---
            bool bCanAttack = true; // Replace with your actual flag from StatsFrag or elsewhere

            if (TargetFrag.bHasValidTarget && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::Chase);
                continue; // Switch state, process next entity
            }

            // --- Update Timer (Only if no target found/chasing) ---
            StateFrag.StateTimer += ExecutionInterval; // Modification stays here

            // --- Check for Returning to Patrol ---
            bool bHasPatrolRoute = PatrolFrag.CurrentWaypointIndex != INDEX_NONE;
            bool bIsOnPlattform = false; // Replace with your actual flag

            // Check member variables bSetUnitsBackToPatrol and SetUnitsBackToPatrolTime exist on 'this' processor
            if (!bIsOnPlattform && !StateFrag.SwitchingState && PatrolFrag.bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= PatrolFrag.SetUnitsBackToPatrolTime)
            {
                StateFrag.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::PatrolRandom);
                continue; // Switch state, process next entity
            }

            // --- Else: Stay Idle ---

        } // End Entity Loop
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
