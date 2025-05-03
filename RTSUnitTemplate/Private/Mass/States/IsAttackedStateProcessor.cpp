// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/IsAttackedStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"


UIsAttackedStateProcessor::UIsAttackedStateProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UIsAttackedStateProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::All); // Nur für Entities in diesem Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Dauer lesen

	// Optionale, aber oft nützliche Fragmente (nicht unbedingt für DIESE Logik nötig)
	// EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	// EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);

	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void UIsAttackedStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UIsAttackedStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_UIsAttackedStateProcessor_Execute); // Performance-Messung (optional)

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
        [&PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];

            // --- Update Timer ---
            StateFrag.StateTimer += DeltaTime; // Modification stays here

            // --- Check if duration exceeded ---
            // Assumes IsAttackedDuration is a member of FMassCombatStatsFragment
            if (StateFrag.StateTimer > StatsFrag.IsAttackedDuration)
            {
                //UE_LOG(LogTemp, Log, TEXT("Entity %d:%d IsAttacked duration ended. Signaling Run."), Entity.Index, Entity.SerialNumber);

                // Queue Run signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder);

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
