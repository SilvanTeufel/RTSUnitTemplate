#include "Mass/Signals/UnitSignalingProcessor.h"
#include "MassSimulationSubsystem.h" // IMPORTANT: Include for the delegate system
#include "Mass/Signals/MassUnitSpawnerSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "Characters/Unit/UnitBase.h"
#include "GameModes/RTSGameModeBase.h"
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
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    UWorld* World = GetWorld();

    if (!World) return;
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
        // Add the newly spawned actors to our internal queue.
        ActorsToCreateThisFrame.Append(PendingActors);
    }
}

// This function is called by the delegate system at a safe time.
void UUnitSignalingProcessor::CreatePendingEntities(const float DeltaTime)
{

    UWorld* World = GetWorld();

    if (!World) return;
    
    const float Now = World->GetTimeSeconds();

    if (Now <= 2.f) return;

    
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (!NavSys) return;
    
    if (ActorsToCreateThisFrame.IsEmpty())
    {
        // UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!ActorsToCreateThisFrame!!!!!!!"));
        return;
    }
    /*
    if (!bIsNavigationReady)
    {
        // Getting a valid default NavData instance is a much better indicator 
        // that the system is ready for pathfinding queries.
        if (NavSys->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfEmpty::DontCreate) != nullptr)
        {
            // It's ready! Set the flag so we don't check again.
            bIsNavigationReady = true;
        }
        else
        {
            // Nav system exists, but NavData isn't available yet. 
            // This is common at game start. Abort this frame's execution and try again next time.
            UE_LOG(LogTemp, Log, TEXT("UUnitMovementProcessor: Waiting for Navigation Data to become available..."));
            return; 
        }
    }
    */
    // It is now SAFE to call synchronous creation functions.
    for (AUnitBase* Unit : ActorsToCreateThisFrame)
    {
        if (IsValid(Unit))
        {
            UMassActorBindingComponent* BindingComp = Unit->MassActorBindingComponent;

            if (BindingComp)
            {
                //BindingComp->CreateAndLinkOwnerToMassEntity();
            }

        
           if (BindingComp && BindingComp->bNeedsMassUnitSetup) // !BindingComp->GetEntityHandle().IsValid())
           {
                // We call your original, working function from the binding component.
                BindingComp->CreateAndLinkOwnerToMassEntity();
           } else   if (BindingComp && BindingComp->bNeedsMassBuildingSetup) // !BindingComp->GetEntityHandle().IsValid())
            {
                // We call your original, working function from the binding component.
                BindingComp->CreateAndLinkBuildingToMassEntity();
            }
            
        }
    }

    // Clear the queue for the next frame.
    ActorsToCreateThisFrame.Empty();
}