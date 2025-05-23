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

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
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

            UE_LOG(LogTemp, Log, TEXT("UPatrolIdleStateProcessor NumEntities: %d"), NumEntities);
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
            if (TargetFrag.bHasValidTarget && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::Chase);
                continue; // Exit loop for this entity as we are switching to Chase
            }

            
            float IdleDuration = FMath::FRandRange(PatrolFrag.RandomPatrolMinIdleTime, PatrolFrag.RandomPatrolMaxIdleTime);
        	
            if (StateFrag.StateTimer >= IdleDuration)
            {
                float Roll = FMath::FRand() * 100.0f;
            	
                if (Roll > PatrolFrag.IdleChance)
                {
                   PendingSignals.Emplace(Entity, UnitSignals::PISwitcher);
                   StateFrag.StateTimer = 0.f;
                }
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
