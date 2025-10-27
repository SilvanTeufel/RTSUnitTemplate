// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/MainStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Controller/PlayerController/CustomControllerBase.h"

UMainStateProcessor::UMainStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UMainStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen

	EntityQuery.RegisterWithProcessor(*this);
}

void UMainStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UMainStateProcessor::HandleUpdateSelectionCircle()
{
    UWorld* World = GetWorld();
    
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController(); // Local controller
    if (!PC) return;

    ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
    if (!CustomPC) return;



    AsyncTask(ENamedThreads::GameThread, [CustomPC]()
    {
        CustomPC->UpdateSelectionCircles();
    });
}

void UMainStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling Check ---

    
    TimeSinceLastRunA += Context.GetDeltaTimeSeconds();
    TimeSinceLastRunB += Context.GetDeltaTimeSeconds();
    // --- Handle Update Selection Circle Logic ---
    if (TimeSinceLastRunB >= UpdateCircleInterval)
    {
        TimeSinceLastRunB -= UpdateCircleInterval;
    }
    
    if (TimeSinceLastRunA < ExecutionInterval)
    {
        return; // Skip execution this frame
    }
    // Interval reached, reset timer
    TimeSinceLastRunA -= ExecutionInterval; // Or TimeSinceLastRun = 0.0f;

    // --- Get World and Signal Subsystem (only if interval was met) ---
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager for World consistently
    if (!World) return;

    if (!SignalSubsystem) return;
    // Make a weak pointer copy for safe capture in the async task
    
    
    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable needed
            //UE_LOG(LogTemp, Warning, TEXT("UMainStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable ref needed
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SyncUnitBase, Entity);
            }

            if (StateFrag.BirthTime == TNumericLimits<float>::Max())
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UnitSpawned, Entity);
                }
            }

            
            
            // --- 1. Check CURRENT entity's health ---
            if (StatsFrag.Health <= 0.f)
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Dead, Entity);
                }
                continue; // Skip further checks for this dead entity
            }
        } // End Entity Loop
    }); // End ForEachEntityChunk

}
