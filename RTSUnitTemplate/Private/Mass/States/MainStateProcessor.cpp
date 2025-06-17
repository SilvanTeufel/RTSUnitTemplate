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
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UMainStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
	// Benötigte Fragmente:
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel lesen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)

    //EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    //EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
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
        HandleUpdateSelectionCircle();
        // Reset timer B. Subtracting the interval prevents time drift.
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
    
    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    
    EntityQuery.ForEachEntityChunk(Context,
        [&PendingSignals, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>(); // Mutable needed
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable needed
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable needed
            //UE_LOG(LogTemp, Warning, TEXT("UMainStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable ref needed
            FMassAITargetFragment& TargetFrag = TargetList[i]; // Mutable ref needed
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            FMassMoveTargetFragment& MoveTargetFrag = MoveTargetList[i]; // Mutable ref needed


            // ChunkContext.GetEntityManagerChecked()
            // if (StatsFrag.TeamId == 3)
            // UE::Mass::Debug::LogEntityTags(Entity, EntityManager, World);
            
           // PendingSignals.Emplace(Entity, UnitSignals::UpdateSelectionCircle);
            PendingSignals.Emplace(Entity, UnitSignals::SyncUnitBase);


            if (StateFrag.BirthTime == TNumericLimits<float>::Max())
            {
                PendingSignals.Emplace(Entity, UnitSignals::UnitSpawned);
            }
            // --- 1. Check CURRENT entity's health ---
            if (StatsFrag.Health <= 0.f)
            {
                // Queue Dead signal
                PendingSignals.Emplace(Entity, UnitSignals::Dead);
                continue; // Skip further checks for this dead entity
            }
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
