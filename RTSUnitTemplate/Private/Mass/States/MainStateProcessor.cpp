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

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
            
        //UE_LOG(LogTemp, Log, TEXT("UMain NumEntities: %d"), NumEntities);

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i); // This is the current ("Attacker") entity

            UE::Mass::Debug::LogEntityTags(Entity, EntityManager, this);
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
                // UE_LOG(LogTemp, Log, TEXT("Entity %d:%d is dead, skipping processing."), Entity.Index, Entity.SerialNumber);
                if (SignalSubsystem) SignalSubsystem->SignalEntity(UnitSignals::Dead, Entity);
                StateFrag.StateTimer = 0.f;
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

                    UE_LOG(LogTemp, Error, TEXT("Entity %d:%d: Checking target %d:%d health: %.2f"),
                    Entity.Index, Entity.SerialNumber, TargetEntity.Index, TargetEntity.SerialNumber, TargetStatsPtr->Health);
                    // Check if the target HAS combat stats AND if health is <= 0
                    if (TargetStatsPtr && TargetStatsPtr->Health <= 0.f)
                    {
                        UE_LOG(LogTemp, Error, TEXT("Entity %d:%d found target %d:%d is dead. Clearing target & signaling Run."),
                            Entity.Index, Entity.SerialNumber, TargetEntity.Index, TargetEntity.SerialNumber);

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
                    UE_LOG(LogTemp, Log, TEXT("Entity %d:%d stored target entity %d:%d is invalid. Clearing target."),
                           Entity.Index, Entity.SerialNumber, TargetEntity.Index, TargetEntity.SerialNumber);
                    TargetFrag.bHasValidTarget = false;
                }
            } // End if(TargetFrag.bHasValidTarget)

        } // End for each entity
    }); // End ForEachEntityChunk
}
