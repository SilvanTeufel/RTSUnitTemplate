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
#include "HAL/IConsoleManager.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"

// CVARs for tuning client registration throughput
static TAutoConsoleVariable<int32> CVarRTS_UnitSignaling_BudgetPerTick(
    TEXT("net.RTS.UnitSignaling.BudgetPerTick"),
    64,
    TEXT("Max registry items processed per tick (rolling)."),
    ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_UnitSignaling_InitialBurstBudget(
    TEXT("net.RTS.UnitSignaling.InitialBurstBudget"),
    256,
    TEXT("Startup burst budget per tick for the first N seconds."),
    ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_UnitSignaling_InitialBurstDuration(
    TEXT("net.RTS.UnitSignaling.InitialBurstDuration"),
    3.0f,
    TEXT("Seconds to use the higher initial burst budget after world start."),
    ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_UnitSignaling_CacheRebuildSeconds(
    TEXT("net.RTS.UnitSignaling.CacheRebuildSeconds"),
    0.25f,
    TEXT("Seconds between cache rebuilds for OwnerName/UnitIndex -> Binding map."),
    ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_UnitSignaling_ExecInterval(
    TEXT("net.RTS.UnitSignaling.ExecInterval"),
    0.1f,
    TEXT("Seconds between runs of UnitSignalingProcessor Execute."),
    ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_UnitSignaling_CreationStartDelay(
    TEXT("net.RTS.UnitSignaling.CreationStartDelay"),
    10.0f,
    TEXT("Seconds to wait after world start before creating/linking Mass entities. If on server, also respects GameMode's GatherControllerTimer."),
    ECVF_Default);

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
    // Allow CVAR to override execution cadence for faster ramp-up
    const float DesiredInterval = FMath::Max(0.02f, CVarRTS_UnitSignaling_ExecInterval.GetValueOnGameThread());
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < DesiredInterval)
    {
        return; 
    }
    TimeSinceLastRun -= DesiredInterval;
    
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

    // Optional diagnostic: log counts to help find missing registrations
   	static TAutoConsoleVariable<int32> CVarRTS_UnitSignaling_LogCounts(
        TEXT("net.RTS.UnitSignaling.LogCounts"),
        0,
        TEXT("Log diagnostic counts in UnitSignalingProcessor Execute (0=off,1=on)."),
        ECVF_Default);

    if (CVarRTS_UnitSignaling_LogCounts.GetValueOnGameThread() > 0)
    {
        int32 LiveUnits = 0;
        int32 Bound = 0;
        for (TActorIterator<AUnitBase> It(World); It; ++It)
        {
            ++LiveUnits;
            if (UMassActorBindingComponent* B = It->MassActorBindingComponent)
            {
                if (B->GetMassEntityHandle().IsValid())
                {
                    ++Bound;
                }
            }
        }
        int32 RegistryCount = 0;
        int32 AgentsCount = 0;
        if (URTSWorldCacheSubsystem* CacheSubDbg = World->GetSubsystem<URTSWorldCacheSubsystem>())
        {
            if (AUnitRegistryReplicator* RegDbg = CacheSubDbg->GetRegistry(false))
            {
                RegistryCount = RegDbg->Registry.Items.Num();
            }
            if (AUnitClientBubbleInfo* BubbleDbg = CacheSubDbg->GetBubble(false))
            {
                AgentsCount = BubbleDbg->Agents.Items.Num();
            }
        }
        UE_LOG(LogTemp, Log, TEXT("[UnitSignaling] Counts: Units=%d Bound=%d Registry=%d Agents=%d (World=%s)"),
            LiveUnits, Bound, RegistryCount, AgentsCount, *World->GetName());
    }

    if (bIsClient && RTSReplicationSettings::GetReplicationMode() == RTSReplicationSettings::Mass)
    {
        URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>();
        if (CacheSub)
        {
            // Rebuild binding cache at throttled interval
            // Cache rebuild interval is configurable
            CacheSub->RebuildBindingCacheIfNeeded(CVarRTS_UnitSignaling_CacheRebuildSeconds.GetValueOnGameThread());
            if (AUnitRegistryReplicator* Registry = CacheSub->GetRegistry(false))
            {
                const TArray<FUnitRegistryItem>& Items = Registry->Registry.Items;
                static int32 RollingIndex = 0;

                const int32 Num = Items.Num();
                if (Num > 0)
                {
                    const float WorldTime = World->GetTimeSeconds();
                    const float BurstDuration = CVarRTS_UnitSignaling_InitialBurstDuration.GetValueOnGameThread();
                    const bool bInBurst = (WorldTime < BurstDuration);
                    const int32 Budget = bInBurst ? CVarRTS_UnitSignaling_InitialBurstBudget.GetValueOnGameThread()
                                                  : CVarRTS_UnitSignaling_BudgetPerTick.GetValueOnGameThread();
                    const int32 ToProcess = FMath::Clamp(Budget, 1, Num);
                    const int32 Start = (RollingIndex % Num);
                    int32 Processed = 0;
                    for (; Processed < ToProcess; ++Processed)
                    {
                        const int32 Idx = (Start + Processed) % Num;
                        const FUnitRegistryItem& It = Items[Idx];
                        UMassActorBindingComponent* Bind = nullptr;
                        if (It.UnitIndex != INDEX_NONE)
                        {
                            Bind = CacheSub->FindBindingByUnitIndex(It.UnitIndex);
                        }
                        if (!Bind)
                        {
                            Bind = CacheSub->FindBindingByOwnerName(It.OwnerName);
                        }
                        if (Bind)
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

    float StartDelay = CVarRTS_UnitSignaling_CreationStartDelay.GetValueOnGameThread();

    // On server, we can sync with GameMode's GatherControllerTimer
    if (World->GetNetMode() != NM_Client)
    {
        if (const ARTSGameModeBase* GM = World->GetAuthGameMode<ARTSGameModeBase>())
        {
            StartDelay = FMath::Max(StartDelay, static_cast<float>(GM->GatherControllerTimer));
        }
    }
    else
    {
        // On client, as an additional safety, we can check if we've received the registry yet.
        // This ensures we don't create local entities too far ahead of the server's authoritative registration.
        if (URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>())
        {
            if (CacheSub->GetRegistry(false) == nullptr)
            {
                // Registry not yet replicated; wait a bit longer unless we are past a hard timeout (e.g. 10s)
                if (Now < 10.f)
                {
                    return;
                }
            }
        }
    }

    if (Now <= StartDelay) return;


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