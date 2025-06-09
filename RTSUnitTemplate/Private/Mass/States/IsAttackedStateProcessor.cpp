// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/IsAttackedStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

UIsAttackedStateProcessor::UIsAttackedStateProcessor(): EntityQuery()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UIsAttackedStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::All); // Nur für Entities in diesem Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Dauer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	// Optionale, aber oft nützliche Fragmente (nicht unbedingt für DIESE Logik nötig)
	// EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	// EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);

	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void UIsAttackedStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UIsAttackedStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //QUICK_SCOPE_CYCLE_COUNTER(STAT_UIsAttackedStateProcessor_Execute); // Performance-Messung (optional)

    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    // --- Get World and Signal Subsystem once ---
    UWorld* World = GetWorld(); // Assumes processor inherits from UObject or similar providing GetWorld()
    if (!World) return;

    if (!SignalSubsystem) return;
    // Make a weak pointer copy for safe capture in the async task


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference.
        // Do NOT capture LocalSignalSubsystem directly here.
        [this, &PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            // --- Update Timer ---
            StateFrag.StateTimer += ExecutionInterval; // Modification stays here

            // --- Check if duration exceeded ---
            // Assumes IsAttackedDuration is a member of FMassCombatStatsFragment
            if (StateFrag.StateTimer > StatsFrag.IsAttackedDuration && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                //UE_LOG(LogTemp, Log, TEXT("Entity %d:%d IsAttacked duration ended. Signaling Run."), Entity.Index, Entity.SerialNumber);

                if (TargetFrag.bHasValidTarget)
                {
                     // Queue Chase signal instead of sending directly
                    PendingSignals.Emplace(Entity, UnitSignals::Chase);
                    // Note: Don't continue here if you might want IdlePatrolSwitcher below?
                    // Logic seems to imply either Chase OR IdlePatrolSwitcher.
                    // If Chase is found, we typically wouldn't also signal IdlePatrolSwitcher.
                    continue; // Exit loop for this entity as we are switching to Chase
                }else
                {
                   PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder);
                }
                
                // Queue Run signal instead of sending directly
                // Reset timer or other state if needed upon leaving IsAttacked
                // StateFrag.StateTimer = 0.0f; // Example reset - keep here if needed

                // Continue to next entity as state is changing
                continue;
            }
            // --- Else: Still in IsAttacked state, do nothing else ---
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
