#include "Mass/States/PatrolIdleStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

UPatrolIdleStateProcessor::UPatrolIdleStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UPatrolIdleStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadWrite); // Idle-Zeiten lesen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Velocity auf 0
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UPatrolIdleStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UPatrolIdleStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling Check ---
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    // --- Get World and Signal Subsystem (only if interval was met) ---
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager for World
    if (!World) return;

    if (!SignalSubsystem) return;

    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context, 
        // Capture PendingSignals by reference.
        // Do NOT capture LocalSignalSubsystem directly here.
        [this, &PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto PatrolList = ChunkContext.GetMutableFragmentView<FMassPatrolFragment>(); // Keep mutable if needed
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>(); // Mutable for stopping
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Keep mutable if needed
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        // const float DeltaTime = ChunkContext.GetDeltaTimeSeconds(); // Not used in loop?
        // const float CurrentWorldTime = ChunkContext.GetWorld()->GetTimeSeconds(); // Not used in loop?
        // Using ExecutionInterval for timer might be more consistent if Execute might skip frames
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer update
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            FMassPatrolFragment& PatrolFrag = PatrolList[i]; // Keep reference if needed
            FMassVelocityFragment& Velocity = VelocityList[i]; // Mutable for stopping
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Keep reference if needed
            const FMassCombatStatsFragment& Stats = StatsList[i];

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Stop Movement & Update Timer ---
            Velocity.Value = FVector::ZeroVector; // Modification stays here
            StateFrag.StateTimer += ExecutionInterval; // Modification stays here

            // --- Check for Valid Target ---
            if (TargetFrag.bHasValidTarget)
            {
                 // Queue Chase signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Chase);
                // Note: Don't continue here if you might want IdlePatrolSwitcher below?
                // Logic seems to imply either Chase OR IdlePatrolSwitcher.
                // If Chase is found, we typically wouldn't also signal IdlePatrolSwitcher.
                continue; // Exit loop for this entity as we are switching to Chase
            }
            else // --- No Valid Target ---
            {
                 // Queue IdlePatrolSwitcher signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::IdlePatrolSwitcher);
                // Continue processing other entities in the chunk
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
