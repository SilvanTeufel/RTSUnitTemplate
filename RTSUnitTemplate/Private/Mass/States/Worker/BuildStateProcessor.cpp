// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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

UBuildStateProcessor::UBuildStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UBuildStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Update StateTimer
    // Read-only fragments needed by the external system handling SpawnBuildingRequest signal:
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadWrite); // For BuildAreaPosition
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // For TeamID

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.RegisterWithProcessor(*this);
}

void UBuildStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    const UWorld* World = EntityManager.GetWorld();

    if (!World) { return; } // Early exit if world is invalid

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World](FMassExecutionContext& Context)
    {
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
        const TArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetMutableFragmentView<FMassWorkerStatsFragment>();

        const int32 NumEntities = Context.GetNumEntities();

        //UE_LOG(LogTemp, Log, TEXT("UBuildStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FMassWorkerStatsFragment WorkerStats = WorkerStatsList[i];
            

            if (WorkerStats.BuildingAvailable || !WorkerStats.BuildingAreaAvailable)
            {
                AIState.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SetUnitStatePlaceholder, Entity);
                }
                continue; // Skip this entity
            }

            const float PreviousStateTimer = AIState.StateTimer;
            AIState.StateTimer += ExecutionInterval;

            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SyncCastTime, Entity);
            }
            // --- Completion Check ---
            if (AIState.StateTimer >= WorkerStats.BuildTime && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SpawnBuildingRequest, Entity);
                }
                continue; // Skip to next entity in chunk
            }

        } // End loop through entities
    }); // End ForEachEntityChunk

}
