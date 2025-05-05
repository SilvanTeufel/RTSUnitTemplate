// In IdleStateProcessor.cpp

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

// ...

UIdleStateProcessor::UIdleStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UIdleStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);

    // Benötigte Fragmente:
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly);
    // === KORREKTUR HIER ===
    // Timer ist jetzt Teil von FMassAIStateFragment
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // ReadWrite für State und Timer

    EntityQuery.RegisterWithProcessor(*this);
}

void UIdleStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
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

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
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
            if (TargetFrag.bHasValidTarget && bCanAttack)
            {
                // Queue Chase signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Chase);
                // Reset timer or other state if needed upon leaving Idle
                // StateFrag.StateTimer = 0.0f; // Example reset - keep here if needed
                continue; // Switch state, process next entity
            }

            // --- Update Timer (Only if no target found/chasing) ---
            StateFrag.StateTimer += ExecutionInterval; // Modification stays here

            // --- Check for Returning to Patrol ---
            bool bHasPatrolRoute = PatrolFrag.CurrentWaypointIndex != INDEX_NONE;
            bool bIsOnPlattform = false; // Replace with your actual flag

            // Check member variables bSetUnitsBackToPatrol and SetUnitsBackToPatrolTime exist on 'this' processor
            if (!bIsOnPlattform && PatrolFrag.bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= PatrolFrag.SetUnitsBackToPatrolTime)
            {
                // Queue PatrolRandom signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::PatrolRandom);

                // Reset timer or other state if needed upon leaving Idle
                // StateFrag.StateTimer = 0.0f; // Example reset - keep here if needed
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
