// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToResourceExtractionStateProcessor.h" // Header for this processor
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"     // For FTransformFragment
#include "MassMovementFragments.h"   // For FMassMoveTargetFragment, FMassVelocityFragment (optional but good practice)
#include "MassSignalSubsystem.h"
#include "Async/Async.h"

// --- Include your specific project Fragments, Tags, and Signals ---
#include "Mass/UnitMassTag.h"       // Contains State Tags (FMassStateGoToResourceExtractionTag)
#include "Mass/Signals/MySignals.h"


UGoToResourceExtractionStateProcessor::UGoToResourceExtractionStateProcessor(): EntityQuery()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UGoToResourceExtractionStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::All);

    // Fragments needed:
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);       // Read TargetEntity, LastKnownLocation, bHasValidTarget
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);         // Read current location for distance check
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);    // Read RunSpeed
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);   // Read ResourceArrivalDistance
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);    // Write new move target / stop movement
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // Might read velocity to see if stuck? Optional.


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToResourceExtractionStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //QUICK_SCOPE_CYCLE_COUNTER(STAT_UGoToResourceExtractionStateProcessor_Execute);
    //TRACE_CPUPROFILER_EVENT_SCOPE(UGoToResourceExtractionStateProcessor_Execute);
    
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    //UWorld* World = Context.GetWorld(); // Get World via Context
   // if (!World) return;

    if (!SignalSubsystem) return;

    
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(Context,
        // Capture World for helper functions
        [this, &PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;
            
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

            //UE_LOG(LogTemp, Log, TEXT("UGoToResourceExtractionStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {

            
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& AIState = AIStateList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];

            AIState.StateTimer += ExecutionInterval;
            // --- 1. Check Target Validity ---

   
            if (!WorkerStatsFrag.ResourceAvailable)
            {
                // Target is lost or invalid. Signal to go idle or find a new task.
                PendingSignals.Emplace(Entity, UnitSignals::GoToBase); // Use appropriate signal
                continue;
            }

            const float DistanceToTargetCenter = FVector::Dist(Transform.GetLocation(), WorkerStatsFrag.ResourcePosition);

            /*
            UE_LOG(
                    LogTemp, 
                    Log, 
                    TEXT("DistanceToTargetCenter = %.2f, ArrivalThreshold = %.2f // SwitchingState %d"), 
                    DistanceToTargetCenter, 
                    WorkerStatsFrag.ResourceArrivalDistance,
                    AIState.SwitchingState
                );*/
            
            if (DistanceToTargetCenter <= WorkerStatsFrag.ResourceArrivalDistance && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::ResourceExtraction); // Use appropriate signal name
            }
            
        }
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
                    }
                }
            });
        }
    }
}
