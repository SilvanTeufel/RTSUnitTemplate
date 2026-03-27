// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/ResourceExtractionStateProcessor.h" // Header for this processor
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"     // For FTransformFragment (if needed later)
#include "MassMovementFragments.h"   // For FMassVelocityFragment
#include "MassSignalSubsystem.h"
#include "Async/Async.h"            // Required for AsyncTask
#include "Controller/PlayerController/CameraControllerBase.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"
#include "Actors/WorkArea.h"
#include "MassActorSubsystem.h"

// Make sure UnitSignals::GoToBase and UnitSignals::Idle (or your equivalents) are defined and accessible

// Ensure FMassSignalPayload is defined (it was in your UnitFragments.h snippet)
// struct FMassSignalPayload { ... }; // No need to redefine if included via UnitFragments.h


UResourceExtractionStateProcessor::UResourceExtractionStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone | (int32)EProcessorExecutionFlags::Client;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UResourceExtractionStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    // Query for entities that are in the Resource Extraction state
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::All);

    // Fragments needed for the logic:
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);     // Read/Write StateTimer
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);   // Read ResourceExtractionTime
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);      // Read target validity (bHasValidTarget) and target entity handle if needed
   // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);     // Write Velocity to stop movement
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);

    // Requirements for Client Signaling:
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

    // Ensure all entities processed by this will have these fragments.
    // Consider adding EMassFragmentPresence::All checks if necessary,
    // though processors usually run *after* state setup ensures fragments exist.

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UResourceExtractionStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());

    if (SignalSubsystem)
    {
        SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateResourceScale)
            .AddUObject(this, &UResourceExtractionStateProcessor::HandleUpdateResourceScale);
    }
}

void UResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    
    if (!SignalSubsystem) return;

    UWorld* World = EntityManager.GetWorld();
    if (!World) return;

    if (World && World->IsNetMode(NM_Client))
    {
        ClientExecute(Context);
    }
    else
    {
        ServerExecute(Context);
    }

    TimeSinceLastRun -= ExecutionInterval;
}

void UResourceExtractionStateProcessor::ServerExecute(FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];

            if (!WorkerStatsFrag.ResourceAvailable && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBase, Entity);
                }
                continue;
            }
            
            StateFrag.StateTimer += ExecutionInterval;
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SyncCastTime, Entity);
            }
           
            if (StateFrag.StateTimer >= WorkerStatsFrag.ResourceExtractionTime && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GetResource, Entity);
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UpdateResourceScale, Entity);
                }
                continue;
            }else if (StateFrag.StateTimer >= WorkerStatsFrag.ResourceExtractionTime*1.5f && StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = false;
            }
        }
    });
}

void UResourceExtractionStateProcessor::ClientExecute(FMassExecutionContext& Context)
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(PC);

    if (CameraPC && CameraPC->bIsAi)
    {
        return;
    }

    TArray<FMassEntityHandle> EntitiesToSignal;
    TArray<FMassEntityHandle> ScaleEntitiesToSignal;

    EntityQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto VisibilityList = ChunkContext.GetFragmentView<FMassVisibilityFragment>();
        const auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassVisibilityFragment& Vis = VisibilityList[i];

            if (Vis.bIsOnViewport)
            {
                EntitiesToSignal.Add(ChunkContext.GetEntity(i));
            }
            
            FMassAIStateFragment& StateFrag = StateList[i];
            StateFrag.StateTimerClient += ExecutionInterval;
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];

            if (StateFrag.StateTimerClient >= (WorkerStatsFrag.ResourceExtractionTime-ExecutionInterval))
            {
                StateFrag.StateTimerClient = 0.f;
                ScaleEntitiesToSignal.Add(ChunkContext.GetEntity(i));
            }
        }
    });

    if (EntitiesToSignal.Num() > 0 && SignalSubsystem)
    {
        SignalSubsystem->SignalEntities(UnitSignals::ResourceExtraction, EntitiesToSignal);
    }

    if (ScaleEntitiesToSignal.Num() > 0 && SignalSubsystem)
    {
        SignalSubsystem->SignalEntities(UnitSignals::UpdateResourceScale, ScaleEntitiesToSignal);
    }
}

void UResourceExtractionStateProcessor::HandleUpdateResourceScale(FName SignalName, TArrayView<const FMassEntityHandle> Entities)
{
    TArray<FMassEntityHandle> EntitiesArray(Entities.GetData(), Entities.Num());
    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy = MoveTemp(EntitiesArray)]()
    {
        if (!EntitySubsystem) return;
        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityActive(Entity)) continue;

            FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
            if (!ActorFrag) continue;

            AActor* Actor = ActorFrag->GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!UnitBase || !UnitBase->ResourcePlace || !IsValid(UnitBase->ResourcePlace)) continue;

            AWorkArea* WA = UnitBase->ResourcePlace;

            float Ratio = WA->MaxAvailableResourceAmount > KINDA_SMALL_NUMBER
                ? WA->AvailableResourceAmount / WA->MaxAvailableResourceAmount : 0.f;
            float ScaleFactor = FMath::Lerp(0.4f, 1.0f, FMath::Clamp(Ratio, 0.f, 1.f));

            WA->SetActorScale3D(WA->OriginalActorScale * FVector(ScaleFactor));
        }
    });
}
