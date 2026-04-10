// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/BuildStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"     // For FMassSignalPayload and sending signals
#include "Async/Async.h"             // For AsyncTask
#include "Engine/World.h"            // For UWorld
#include "Templates/SubclassOf.h"    // For TSubclassOf check
#include "MassCommonFragments.h"

// Your project specific includes
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

// No Actor includes, no Movement includes needed

UBuildStateProcessor::UBuildStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
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
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
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

    const bool bIsClient = (World->GetNetMode() == NM_Client);

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, bIsClient](FMassExecutionContext& ChunkContext)
    {
        auto AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto WorkerStatsList = ChunkContext.GetMutableFragmentView<FMassWorkerStatsFragment>();
        const int32 NumEntities = ChunkContext.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, AIStateList[i], WorkerStatsList[i], ChunkContext.GetEntity(i), i);
            }
            else
            {
                ServerExecute(EntityManager, ChunkContext, AIStateList[i], WorkerStatsList[i], ChunkContext.GetEntity(i), i);
            }
        }
    });

}

void UBuildStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    if (WorkerStats.BuildingAvailable || !WorkerStats.BuildingAreaAvailable)
    {
        AIState.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToBase, Entity);
        }
        return;
    }

    AIState.StateTimer += ExecutionInterval;
    AIState.DeltaTime = ExecutionInterval;

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
    }

    // Rotation Logic
    auto TransformList = Context.GetMutableFragmentView<FTransformFragment>();
    FTransform& CurrentTransform = TransformList[EntityIdx].GetMutableTransform();
    FVector LookDir = (WorkerStats.BuildAreaPosition - CurrentTransform.GetLocation());
    LookDir.Z = 0.f;
    if (!LookDir.IsNearlyZero())
    {
        FQuat TargetRotation = FRotationMatrix::MakeFromX(LookDir).ToQuat();
        CurrentTransform.SetRotation(TargetRotation);
    }
}

void UBuildStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    // Rotation logic on client is now handled by UActorTransformSyncProcessor
}
