// Fill out your copyright notice in the Description page of Project Settings.

#include "Mass/States/Worker/BuildStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"     // For FMassSignalPayload and sending signals
#include "Async/Async.h"             // For AsyncTask
#include "Engine/World.h"            // For UWorld
#include "Templates/SubclassOf.h"    // For TSubclassOf check

// Your project specific includes
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

// No Actor includes, no Movement includes needed

UBuildStateProcessor::UBuildStateProcessor()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    // No specific execution order dependency for this state logic
}

void UBuildStateProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Update StateTimer
    // Read-only fragments needed by the external system handling SpawnBuildingRequest signal:
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadWrite); // For BuildAreaPosition
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // For TeamID

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);

    EntityQuery.RegisterWithProcessor(*this);
}

void UBuildStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaSeconds = Context.GetDeltaTimeSeconds();
    const UWorld* World = EntityManager.GetWorld();

    if (!World) { return; } // Early exit if world is invalid

    if (!SignalSubsystem) return;
    // Use FMassSignalPayload (defined by Mass framework)
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, DeltaSeconds, World, &PendingSignals](FMassExecutionContext& Context)
    {
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
        const TArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetMutableFragmentView<FMassWorkerStatsFragment>();
        // Note: We query for WorkerStats and CombatStats but don't need their views directly in this loop.
        // The query ensures they are present for the system handling the SpawnBuildingRequest signal.

        const int32 NumEntities = Context.GetNumEntities();
            UE_LOG(LogTemp, Warning, TEXT("UBuildStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FMassWorkerStatsFragment WorkerStats = WorkerStatsList[i];
            // --- Pre-check ---
            // Basic validation of essential build parameter. More robust validation assumed external.
            if (WorkerStats.BuildingAvailable) // Check if Building is allready set
            {
                UE_LOG(LogTemp, Warning, TEXT("Entity %d: BuildStateProcessor: Invalid BuildingClass. Queuing GoToResourceExtraction signal."), Entity.Index);
                PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder); // Use appropriate fallback signal FName
                // Signal handler should remove the Build tag.
                continue; // Skip this entity
            }

            const float PreviousStateTimer = AIState.StateTimer;
            AIState.StateTimer += DeltaSeconds;

            // --- Signal Start Build Action (once near beginning) ---
            // Use a small threshold to signal once after entering the state
            /*
            constexpr float StartActionThreshold = 0.05f;
            if (PreviousStateTimer < StartActionThreshold && AIState.StateTimer >= StartActionThreshold)
            {
                 UE_LOG(LogTemp, Log, TEXT("Entity %d: BuildStateProcessor: Queuing signal '%s'."), Entity.Index, *UnitSignals::StartBuildAction.ToString());
                 PendingSignals.Emplace(Entity, UnitSignals::StartBuildAction); // Signal for animation/effects
            }*/

            PendingSignals.Emplace(Entity, UnitSignals::SyncCastTime);
            // --- Completion Check ---
            if (AIState.StateTimer >= WorkerStats.BuildTime)
            {
                UE_LOG(LogTemp, Log, TEXT("Entity %d: BuildStateProcessor: Build complete. Queuing '%s' and '%s' signals."),
                Entity.Index, *UnitSignals::SpawnBuildingRequest.ToString(), *UnitSignals::GoToResourceExtraction.ToString());
                
                PendingSignals.Emplace(Entity, UnitSignals::SpawnBuildingRequest);
                //PendingSignals.Emplace(Entity, UnitSignals::GoToResourceExtraction);
                continue; // Skip to next entity in chunk
            }

        } // End loop through entities
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSignalPayload& Payload : SignalsToSend)
                    {
                        if (!Payload.SignalName.IsNone())
                        {
                           StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                        }
                        else { UE_LOG(LogTemp, Error, TEXT("AsyncTask Signal Sender: Cannot send signal with NAME_None for Entity %d."), Payload.TargetEntity.Index); }
                    }
                }
                else { UE_LOG(LogTemp, Warning, TEXT("AsyncTask Signal Sender: UMassSignalSubsystem became invalid.")); }
            });
        }
        else { UE_LOG(LogTemp, Error, TEXT("UBuildStateProcessor: Cannot find UMassSignalSubsystem to send %d signals."), PendingSignals.Num()); }
    }
}
