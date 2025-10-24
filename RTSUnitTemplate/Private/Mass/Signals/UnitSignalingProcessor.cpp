#include "Mass/Signals/UnitSignalingProcessor.h"
#include "MassSimulationSubsystem.h" // IMPORTANT: Include for the delegate system
#include "Mass/Signals/MassUnitSpawnerSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Mass/MassActorBindingComponent.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "EngineUtils.h"

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

    // Client-side proactive registry reconciliation for Mass replication mode
    const bool bIsClient = World->GetNetMode() == NM_Client;
    if (bIsClient && RTSReplicationSettings::GetReplicationMode() == RTSReplicationSettings::Mass)
    {
        URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>();
        if (CacheSub)
        {
            // Rebuild binding cache at throttled interval
            CacheSub->RebuildBindingCacheIfNeeded(1.0f);
            if (AUnitRegistryReplicator* Registry = CacheSub->GetRegistry(false))
            {
                const TArray<FUnitRegistryItem>& Items = Registry->Registry.Items;
                static int32 RollingIndex = 0;

                const int32 Num = Items.Num();
                if (Num > 0)
                {
                    const int32 BudgetPerTick = 16; // small rolling slice
                    const int32 ToProcess = FMath::Clamp(BudgetPerTick, 1, Num);
                    const int32 Start = (RollingIndex % Num);
                    int32 Processed = 0;
                    for (; Processed < ToProcess; ++Processed)
                    {
                        const int32 Idx = (Start + Processed) % Num;
                        const FUnitRegistryItem& It = Items[Idx];
                        if (UMassActorBindingComponent* Bind = CacheSub->FindBindingByOwnerName(It.OwnerName))
                        {
                            Bind->RequestClientMassLink();
                        }
                    }
                    RollingIndex = (Start + Processed) % Num;
                }
            }
        }
    }
}

// This function is called by the delegate system at a safe time.
void UUnitSignalingProcessor::CreatePendingEntities(const float DeltaTime)
{

    UWorld* World = GetWorld();

    if (!World) return;
    
    const float Now = World->GetTimeSeconds();

    if (Now <= 2.f) return;
    
    if (ActorsToCreateThisFrame.IsEmpty())
    {
        // UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!ActorsToCreateThisFrame!!!!!!!"));
        return;
    }
	
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
           }else if (BindingComp && BindingComp->bNeedsMassBuildingSetup) // !BindingComp->GetEntityHandle().IsValid())
           {
                // We call your original, working function from the binding component.
              BindingComp->CreateAndLinkBuildingToMassEntity();
           }
            
        }
    }

    // Clear the queue for the next frame.
    ActorsToCreateThisFrame.Empty();
}