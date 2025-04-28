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

    UMassSignalSubsystem* LocalSignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!LocalSignalSubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("UMainStateProcessor: Could not get SignalSubsystem!"));
        return;
    }
    // Make a weak pointer copy for safe capture in the async task
    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = LocalSignalSubsystem;


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
                        PendingSignals.Emplace(Entity, UnitSignals::Run);

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

/*
void UMainStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        // Noch nicht Zeit, diesen Frame überspringen
        return;
    }

    // --- Intervall erreicht, Logik ausführen ---

    // 3. Timer zurücksetzen (Interval abziehen ist genauer als auf 0 setzen)
    TimeSinceLastRun -= ExecutionInterval;
    
    //UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!!!!!!!!!!UMainStateProcessor::Execute!!!!!!!!!!"));
    UWorld* World = EntityManager.GetWorld();
    if (!World) return;

    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>(); // Get subsystem once

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, World, SignalSubsystem, &EntityManager](FMassExecutionContext& ChunkContext) // Capture EntityManager by reference
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        // Get MUTABLE view for TargetFragment now
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        // Get MUTABLE view for MoveTargetFragment
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
            
        //UE_LOG(LogTemp, Log, TEXT("UMain NumEntities: %d"), NumEntities);

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i); // This is the current ("Attacker") entity
            
            // --- Get Fragments for the CURRENT entity ---
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassAITargetFragment& TargetFrag = TargetList[i]; // Mutable now
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            FMassMoveTargetFragment& MoveTargetFrag = MoveTargetList[i]; // Mutable now

            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntity(UnitSignals::SyncUnitBase, Entity); // Use 'Entity' handle
            }
            
            // --- 1. Check CURRENT entity's health first ---
            if (StatsFrag.Health <= 0.f)
            {
                // If current entity is dead, potentially signal (might be redundant) and skip further processing for it
                //UE_LOG(LogTemp, Error, TEXT("Entity %d:%d is dead, skipping processing."), Entity.Index, Entity.SerialNumber);
                if (SignalSubsystem) SignalSubsystem->SignalEntity(UnitSignals::Dead, Entity);
                continue; // Move to the next entity in the chunk
            }

            // --- 2. Check TARGET entity's health ---
            // Check if the current entity has a valid target reference
            if (TargetFrag.bHasValidTarget && TargetFrag.TargetEntity.IsSet())
            {
                const FMassEntityHandle TargetEntity = TargetFrag.TargetEntity;

                // Check if the target entity handle is still valid in the world
                if (EntityManager.IsEntityValid(TargetEntity))
                {
                    // Attempt to get the target's health from its CombatStats fragment
                    const FMassCombatStatsFragment* TargetStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity);
                    // Check if the target HAS combat stats AND if health is <= 0
                    if (TargetStatsPtr && TargetStatsPtr->Health <= 0.f)
                    {
                        // Signal the CURRENT entity (Attacker) to Run
                        if (SignalSubsystem)
                        {
                            SignalSubsystem->SignalEntity(UnitSignals::Run, Entity); // Use 'Entity' handle
                        }

                        // Clear the target flag on the CURRENT entity's fragment
                        TargetFrag.bHasValidTarget = false;
                        // Optionally clear other fields: TargetFrag.Clear();

                        ChunkContext.Defer().AddTag<FMassStateDetectTag>(Entity);
                        // Update the CURRENT entity's move target using ITS OWN fragments
                        UpdateMoveTarget(MoveTargetFrag, StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
                    }
                    // Optional: Else block if target is alive and valid?
                    // else { // Target is alive }

                }
                else // TargetFrag.TargetEntity is no longer a valid entity handle
                {
                    // If the stored target entity is invalid, clear the target fragment state
                    TargetFrag.bHasValidTarget = false;
                }
            } // End if(TargetFrag.bHasValidTarget)

        } // End for each entity
    }); // End ForEachEntityChunk
}*/
