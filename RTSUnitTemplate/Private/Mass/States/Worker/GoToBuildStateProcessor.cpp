// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToBuildStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"     // For FMassSignalPayload and sending signals
#include "Async/Async.h"
#include "Engine/World.h"

// Your project specific includes
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

// No Actor/Component includes needed in this file anymore


UGoToBuildStateProcessor::UGoToBuildStateProcessor(): EntityQuery()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToBuildStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // NO FMassActorFragment
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly); // Contains target pos/radius now
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);       // For speed
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::All);


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBuildStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    UWorld* World = EntityManager.GetWorld();

    if (!World)
    {
        //UE_LOG(LogTemp, Error, TEXT("UGoToBuildStateProcessor: Cannot execute without a valid UWorld."));
        return;
    }

    if (!SignalSubsystem) return;
    
    // Use the engine/Mass provided FMassSignalPayload struct
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &PendingSignals](FMassExecutionContext& Context)
    {
        // --- Get Fragment Views ---
        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetFragmentView<FMassWorkerStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = Context.GetFragmentView<FMassCombatStatsFragment>();

        const int32 NumEntities = Context.GetNumEntities();
            
        //UE_LOG(LogTemp, Log, TEXT("UGoToBuildStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];

            // Increment state timer
            AIState.StateTimer += ExecutionInterval;
            
    
            // Basic validation of data from fragment (more robust checks should be external)
            if (WorkerStats.BuildingAvailable || !WorkerStats.BuildingAreaAvailable) // Example basic check
            {
                 PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder); // Use appropriate fallback signal FName
                 continue;
            }

            const float DistanceToTargetCenter = FVector::Dist(CurrentTransform.GetLocation(), WorkerStats.BuildAreaPosition);

            MoveTarget.DistanceToGoal = DistanceToTargetCenter; // Update distance
            if (DistanceToTargetCenter <= WorkerStats.BuildAreaArrivalDistance)
            {
                AIState.SwitchingState = true;

                PendingSignals.Emplace(Entity, UnitSignals::Build); // Use appropriate signal name
                continue;
            }
        } // End loop through entities
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task to Send Queued Signals (Same as before) ---
    if (!PendingSignals.IsEmpty())
    {
      
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSignalPayload& Payload : SignalsToSend) // Iterate FMassSignalPayload
                    {
                        if (!Payload.SignalName.IsNone())
                        {
                           StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                        }
                         else { /* Log error */ }
                    }
                }
                else { /* Log error: Subsystem invalid */ }
            });
        }
        else { /* Log error: Subsystem not found */ }
    }
}
