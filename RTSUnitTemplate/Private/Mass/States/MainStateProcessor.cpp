// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/MainStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

UMainStateProcessor::UMainStateProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UMainStateProcessor::ConfigureQueries()
{
	// Benötigte Fragmente:
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel lesen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)

	EntityQuery.RegisterWithProcessor(*this);
}

void UMainStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UMainStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling Check ---
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; // Skip execution this frame
    }
    // Interval reached, reset timer
    TimeSinceLastRun -= ExecutionInterval; // Or TimeSinceLastRun = 0.0f;

    // --- Get World and Signal Subsystem (only if interval was met) ---
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager for World consistently
    if (!World) return;

    if (!SignalSubsystem) return;
    // Make a weak pointer copy for safe capture in the async task


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference. Capture World & EntityManager for inner logic.
        // Do NOT capture LocalSignalSubsystem directly here.
        [&PendingSignals, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>(); // Mutable needed
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable needed
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable needed

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable ref needed
            FMassAITargetFragment& TargetFrag = TargetList[i]; // Mutable ref needed
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            FMassMoveTargetFragment& MoveTargetFrag = MoveTargetList[i]; // Mutable ref needed

            //UE::Mass::Debug::LogEntityTags(Entity, EntityManager, World);
            // --- Queue Sync Signal ---
            // Always queue this signal if the processor runs for the entity
            PendingSignals.Emplace(Entity, UnitSignals::SyncUnitBase);

            // --- 1. Check CURRENT entity's health ---
            if (StatsFrag.Health <= 0.f)
            {
                // Queue Dead signal
                PendingSignals.Emplace(Entity, UnitSignals::Dead);
                continue; // Skip further checks for this dead entity
            }

            // --- 2. Check TARGET entity's health ---
            if (TargetFrag.bHasValidTarget && TargetFrag.TargetEntity.IsSet())
            {
                const FMassEntityHandle TargetEntity = TargetFrag.TargetEntity;

                // EntityManager access is generally safe within Mass Processors
                if (EntityManager.IsEntityValid(TargetEntity))
                {
                    const FMassCombatStatsFragment* TargetStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity);

                    if (TargetStatsPtr && TargetStatsPtr->Health <= 0.f) // Target is dead
                    {
                        // Queue Run signal for the CURRENT entity
                        PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder);

                        // --- Direct Fragment/Context Modifications Stay Here ---
                        TargetFrag.bHasValidTarget = false; // Modify fragment directly
                        // Optionally TargetFrag.Clear();
                        ChunkContext.Defer().AddTag<FMassStateDetectTag>(Entity); // Use context directly
                        UpdateMoveTarget(MoveTargetFrag, StateFrag.StoredLocation, StatsFrag.RunSpeed, World); // Modify fragment directly
                        // -------------------------------------------------------

                        // Since we switched state to Run, continue to next entity
                        continue;
                    }
                    // else { // Target is alive and valid, do nothing in this block }
                }
                else // Target Entity Handle is invalid
                {
                     // --- Direct Fragment Modification Stays Here ---
                    TargetFrag.bHasValidTarget = false; // Modify fragment directly
                     // Optionally TargetFrag.Clear();
                    // -----------------------------------------------
                    // No signal needed here? Maybe signal Run? Depends on desired behavior.
                    // PendingSignals.Emplace(Entity, UnitSignals::Run);
                }
            } // End if target is set

            // --- Potentially other checks for this entity ---

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
