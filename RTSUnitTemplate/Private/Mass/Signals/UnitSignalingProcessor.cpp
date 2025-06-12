#include "Mass/Signals/UnitSignalingProcessor.h"
#include "MassSimulationSubsystem.h" // IMPORTANT: Include for the delegate system
#include "Mass/Signals/MassUnitSpawnerSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/MassActorBindingComponent.h"

UUnitSignalingProcessor::UUnitSignalingProcessor()
{
    // This flag is essential to ensure Execute() runs even with zero entities.
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UUnitSignalingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSignalingProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    UWorld* World = GetWorld();

    if (!World) return;
    
    UMassSimulationSubsystem* SimSubsystem = World->GetSubsystem<UMassSimulationSubsystem>();
    if (SimSubsystem)
    {
        UE_LOG(LogTemp, Log, TEXT("[UUnitSignalingProcessor] INITIALIZE STARTING..."));
        // Bind our function to the delegate that fires after our processing phase is done.
        PhaseFinishedDelegateHandle = SimSubsystem->GetOnProcessingPhaseFinished(ProcessingPhase).AddUObject(this, &UUnitSignalingProcessor::CreatePendingEntities);
    }

    SpawnerSubsystem = World->GetSubsystem<UMassUnitSpawnerSubsystem>();
}

void UUnitSignalingProcessor::BeginDestroy()
{
    Super::BeginDestroy();
    UWorld* World = GetWorld();

    if (!World) return;
    
    if (UMassSimulationSubsystem* SimSubsystem = World->GetSubsystem<UMassSimulationSubsystem>())
    {
        SimSubsystem->GetOnProcessingPhaseFinished(ProcessingPhase).Remove(PhaseFinishedDelegateHandle);
    }
}

void UUnitSignalingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!Execute!!!!!!!"));
    UWorld* World = GetWorld();

    if (!World) return;
    
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!Execute!!!!!!!"));
    // On the first run, get a pointer to our spawner subsystem.
    if (!SpawnerSubsystem)
    {
        SpawnerSubsystem = World->GetSubsystem<UMassUnitSpawnerSubsystem>();
        if (!SpawnerSubsystem)
        {
            return; 
        }
    }

    TArray<AUnitBase*> PendingActors;
    SpawnerSubsystem->GetAndClearPendingUnits(PendingActors);

    if (!PendingActors.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!Execute222222!!!!!!!"));
        // Add the newly spawned actors to our internal queue.
        ActorsToCreateThisFrame.Append(PendingActors);
    }
}

// This function is called by the delegate system at a safe time.
void UUnitSignalingProcessor::CreatePendingEntities(const float DeltaTime)
{
/*
    // On the first run, get a pointer to our spawner subsystem.
    if (!SpawnerSubsystem)
    {
        SpawnerSubsystem = GetWorld()->GetSubsystem<UMassUnitSpawnerSubsystem>();
        if (!SpawnerSubsystem)
        {
            return; 
        }
    }

    
    TArray<AUnitBase*> PendingActors;
    SpawnerSubsystem->GetAndClearPendingUnits(PendingActors);

    if (!PendingActors.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!CreatePendingEntities000!!!!!!!"));
        // Add the newly spawned actors to our internal queue.
        ActorsToCreateThisFrame.Append(PendingActors);
    }
    */
   // UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!CreatePendingEntities!!!!!!!"));
    if (ActorsToCreateThisFrame.IsEmpty())
    {
        // UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!ActorsToCreateThisFrame!!!!!!!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!Found ActorsToCreateThisFrame!!!!!!!"));
    // It is now SAFE to call synchronous creation functions.
    for (AUnitBase* Unit : ActorsToCreateThisFrame)
    {
        if (IsValid(Unit))
        {
            UMassActorBindingComponent* BindingComp = Unit->MassActorBindingComponent;

            if (BindingComp)
            {
                UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!!!FOUND BindingComp!!!!!!!")); 
            }
            
            if (BindingComp && BindingComp->bNeedsMassUnitSetup) // !BindingComp->GetEntityHandle().IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!!!CreateAndLinkOwnerToMassEntity!!!!!!!"));
                // We call your original, working function from the binding component.
                BindingComp->CreateAndLinkOwnerToMassEntity();
            } else   if (BindingComp && BindingComp->bNeedsMassBuildingSetup) // !BindingComp->GetEntityHandle().IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!!!CreateAndLinkOwnerToMassEntity!!!!!!!"));
                // We call your original, working function from the binding component.
                BindingComp->CreateAndLinkBuildingToMassEntity();
            }
        }
    }

    // Clear the queue for the next frame.
    ActorsToCreateThisFrame.Empty();
}