// Fill out your copyright notice in the Description page of Project Settings.



#include "Mass/Replication/ClientReplicationProcessor.h"
#include "HAL/IConsoleManager.h"

// CVARs for ClientReplicationProcessor (client)
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_EnableCache(
    TEXT("net.RTS.ClientReplication.EnableCache"),
    1,
    TEXT("Enable shared OwnerName->Binding cache to avoid actor scans."),
    ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_ClientReplication_CacheRebuildSeconds(
    TEXT("net.RTS.ClientReplication.CacheRebuildSeconds"),
    1.0f,
    TEXT("Seconds between cache rebuilds when enabled."),
    ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_BudgetPerTick(
    TEXT("net.RTS.ClientReplication.BudgetPerTick"),
    64,
    TEXT("Max reconcile/link/unlink operations per tick on client."),
    ECVF_Default);
// 0=Off, 1=Warn, 2=Verbose
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_LogLevel(
    TEXT("net.RTS.ClientReplication.LogLevel"),
    0,
    TEXT("Logging level: 0=Off, 1=Warn, 2=Verbose."),
    ECVF_Default);
// Debounce window to wait after last registry chunk before unlinking missing entities
static TAutoConsoleVariable<float> CVarRTS_ClientReplication_UnlinkDebounceSeconds(
    TEXT("net.RTS.ClientReplication.UnlinkDebounceSeconds"),
    0.10f,
    TEXT("Seconds to wait after the registry changes before executing unlink reconciliation."),
    ECVF_Default);

#include "MassCommonFragments.h"
#include "MassEntityTypes.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "MassActorSubsystem.h"
#include "MassReplicationFragments.h"
#include "Mass/Replication/UnitReplicationCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Replication/UnitReplicationPayload.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "MassRepresentationFragments.h"
#include "EngineUtils.h"
#include "Mass/MassActorBindingComponent.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

UClientReplicationProcessor::UClientReplicationProcessor()
	: EntityQuery(*this)
{
	// Ensure the processor is auto-registered into the runtime pipeline on clients
	bAutoRegisterWithProcessingPhases = true;
	// THIS IS CRITICAL: This processor should ONLY run on clients
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client;
	// We need to touch AActors and iterate world actors; require GameThread execution
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

 if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
 	{
 		UE_LOG(LogTemp, Log, TEXT("ClientReplicationProcessor: Constructed. Phase=%d"), (int32)ProcessingPhase);
 	}
}

void UClientReplicationProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	UWorld* World = GetWorld();
	const ENetMode Mode = World ? World->GetNetMode() : NM_Standalone;
 if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
	{
		UE_LOG(LogTemp, Log, TEXT("ClientReplicationProcessor: InitializeInternal. World=%s NetMode=%d AutoReg=%d"),
			World ? *World->GetName() : TEXT("<null>"), (int32)Mode, bAutoRegisterWithProcessingPhases ? 1 : 0);
	}
}

void UClientReplicationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FUnitReplicatedTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	// Prediction fragment so we can skip reconciliation while client-side prediction is active
	EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
		
}

void UClientReplicationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (bSkipReplication) return;

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun = 0.f;
	
	static int32 GExecCount = 0;
	UWorld* World = GetWorld();
	if (!World)
	{
  if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
  	{
  		UE_LOG(LogTemp, Warning, TEXT("ClientReplicationProcessor: Execute skipped (no World)"));
  	}
		return;
	}
	// Respect global replication mode: only run in custom Mass mode on clients
	if (RTSReplicationSettings::GetReplicationMode() != RTSReplicationSettings::Mass)
	{
		return;
	}
 if (!World->IsNetMode(NM_Client))
	{
		if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Verbose, TEXT("ClientReplicationProcessor: Execute skipped. NetMode=%d World=%s"), (int32)World->GetNetMode(), *World->GetName());
		}
		return;
	}
	GExecCount++;
	// Update global replication mode from CVAR each tick for runtime switching
	if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2 && GExecCount <= 60)
	{
  UE_LOG(LogTemp, Verbose, TEXT("ClientReplicationProcessor: Execute #%d Time=%.2f (FullReplication=%d)"), GExecCount, World->GetTimeSeconds(), bUseFullReplication ? 1 : 0);
	}

	// 1) Gather existing client-side NetIDs so we can detect missing entities
	TSet<uint32> ExistingIDs;
	int32 TotalEntities = 0;
	int32 TotalChunks = 0;
	EntityQuery.ForEachEntityChunk(Context, [&ExistingIDs, &TotalEntities, &TotalChunks](FMassExecutionContext& GatherCtx)
	{
		const int32 Num = GatherCtx.GetNumEntities();
		TotalEntities += Num;
		TotalChunks++;
		const TConstArrayView<FMassNetworkIDFragment> NetIDList = GatherCtx.GetFragmentView<FMassNetworkIDFragment>();
		for (int32 i = 0; i < Num; ++i)
		{
			ExistingIDs.Add(NetIDList[i].NetID.GetValue());
		}
	});
 if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2 && GExecCount <= 60)
	{
		UE_LOG(LogTemp, Log, TEXT("ClientReplicationProcessor: Query matched %d entities across %d chunks"), TotalEntities, TotalChunks);
	}

	// 2) We do NOT create entities on the client. Instead we will map NetIDs to already created entities later in this tick.
	//    Throttle registry actor lookup to once per second and cache between ticks.
	AUnitRegistryReplicator* RegistryActor = nullptr;
	TMap<FName, FMassNetworkID> AuthoritativeByOwnerName;
	TMap<int32, FMassNetworkID> AuthoritativeByUnitIndex;
	{
		static TWeakObjectPtr<AUnitRegistryReplicator> GCachedRegistry;
		static double GLastLookup = 0.0;
		UWorld* LocalWorld = GetWorld();
		const double Now = LocalWorld ? LocalWorld->GetTimeSeconds() : 0.0;
		if (GCachedRegistry.IsValid())
		{
			RegistryActor = GCachedRegistry.Get();
		}
		else if (LocalWorld && (Now - GLastLookup) >= 1.0)
		{
			GLastLookup = Now;
			for (TActorIterator<AUnitRegistryReplicator> It(LocalWorld); It; ++It)
			{
				GCachedRegistry = *It;
				break;
			}
			RegistryActor = GCachedRegistry.Get();
		}

		if (RegistryActor)
		{
			for (const FUnitRegistryItem& Item : RegistryActor->Registry.Items)
			{
				AuthoritativeByOwnerName.Add(Item.OwnerName, Item.NetID);
				if (Item.UnitIndex != INDEX_NONE)
				{
					AuthoritativeByUnitIndex.Add(Item.UnitIndex, Item.NetID);
				}
			}
			// Log the mapping we see on the client this tick (limited, verbose only)
			if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
			{
				const int32 Num = RegistryActor->Registry.Items.Num();
				const int32 MaxLog = FMath::Min(25, Num);
				FString MapStr;
				for (int32 i = 0; i < MaxLog; ++i)
				{
					const FUnitRegistryItem& It = RegistryActor->Registry.Items[i];
					if (i > 0) { MapStr += TEXT(" | "); }
					MapStr += FString::Printf(TEXT("%s->%u"), *It.OwnerName.ToString(), It.NetID.GetValue());
				}
				UE_LOG(LogTemp, Verbose, TEXT("ClientRegistry(Tick): %d entries. %s%s"), Num, *MapStr, (Num > MaxLog ? TEXT(" ...") : TEXT("")));
			}
		}
	}

	// Reconcile: ensure client has the same set of entities as the authoritative registry
	// Build quick lookup maps from current client Mass entities
	TMap<uint32, TWeakObjectPtr<AActor>> ByNetID;
	TMap<FName, TWeakObjectPtr<AActor>> ByOwnerName;

	// Build additional authoritative mapping from Bubble payload (client-replicated)
	TMap<FName, FMassNetworkID> BubbleByOwnerName;
	TSet<uint32> BubbleIDs;
		if (URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>())
		{
			if (AUnitClientBubbleInfo* Bubble = CacheSub->GetBubble(false))
			{
				for (const FUnitReplicationItem& Item : Bubble->Agents.Items)
				{
					BubbleByOwnerName.Add(Item.OwnerName, Item.NetID);
					BubbleIDs.Add(Item.NetID.GetValue());
				}
			}
		}
		// Compare Registry vs Bubble mappings by OwnerName and log mismatches (diagnostic)
		if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1 && AuthoritativeByOwnerName.Num() > 0 && BubbleByOwnerName.Num() > 0)
		{
			int32 MismatchCount = 0;
			int32 OnlyInRegistry = 0;
			int32 OnlyInBubble = 0;
			for (const auto& KVP : AuthoritativeByOwnerName)
			{
				const FMassNetworkID* B = BubbleByOwnerName.Find(KVP.Key);
				if (!B)
				{
					OnlyInRegistry++;
				}
				else if (B->GetValue() != KVP.Value.GetValue())
				{
					MismatchCount++;
				}
			}
			for (const auto& KVP : BubbleByOwnerName)
			{
				if (!AuthoritativeByOwnerName.Contains(KVP.Key))
				{
					OnlyInBubble++;
				}
			}
			if (MismatchCount > 0 || OnlyInRegistry > 0 || OnlyInBubble > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("ClientRegistryVsBubble: Mismatch=%d OnlyInReg=%d OnlyInBubble=%d (Reg=%d Bubble=%d)"),
					MismatchCount, OnlyInRegistry, OnlyInBubble, AuthoritativeByOwnerName.Num(), BubbleByOwnerName.Num());
			}
		}
		
		EntityQuery.ForEachEntityChunk(Context, [&ByNetID, &ByOwnerName](FMassExecutionContext& Ctx)
	{
		const int32 Num = Ctx.GetNumEntities();
		const TConstArrayView<FMassNetworkIDFragment> NetIDs = Ctx.GetFragmentView<FMassNetworkIDFragment>();
		const TArrayView<FMassActorFragment> Actors = Ctx.GetMutableFragmentView<FMassActorFragment>();
		for (int32 i = 0; i < Num; ++i)
		{
			if (AActor* Act = Actors[i].GetMutable())
			{
				ByNetID.Add(NetIDs[i].NetID.GetValue(), Act);
				ByOwnerName.Add(Act->GetFName(), Act);
			}
		}
	});
	// Build set of registry NetIDs for fast contains checks and track stability
	TSet<uint32> RegistryIDs;
	double NowSeconds = World->GetTimeSeconds();
	static TMap<const AUnitRegistryReplicator*, uint64> GLastRegistrySignatureByActor;
	static TMap<const AUnitRegistryReplicator*, double> GLastRegistryChangeTimeByActor;
	bool bRegistryStable = true; // default stable when no registry or no change detected
	if (RegistryActor)
	{
		uint64 Hash = 1469598103934665603ull; // FNV-1a 64-bit
		for (const FUnitRegistryItem& It : RegistryActor->Registry.Items)
		{
			const uint32 IdVal = It.NetID.GetValue();
			RegistryIDs.Add(IdVal);
			Hash ^= (uint64)IdVal; Hash *= 1099511628211ull;
		}
		uint64* PrevHash = GLastRegistrySignatureByActor.Find(RegistryActor);
		if (!PrevHash || *PrevHash != Hash)
		{
			GLastRegistrySignatureByActor.Add(RegistryActor, Hash);
			GLastRegistryChangeTimeByActor.Add(RegistryActor, NowSeconds);
		}
		const double* LastChange = GLastRegistryChangeTimeByActor.Find(RegistryActor);
		const float Debounce = CVarRTS_ClientReplication_UnlinkDebounceSeconds.GetValueOnGameThread();
		bRegistryStable = (LastChange == nullptr) || ((NowSeconds - *LastChange) >= Debounce);
	}
	// Build the full authoritative remote ID set as union of Registry and Bubble IDs
	TSet<uint32> FullRemoteIDs = RegistryIDs;
	for (uint32 Bid : BubbleIDs) { FullRemoteIDs.Add(Bid); }
	// Create any missing client entities by OwnerName
	int32 Actions = 0;
	int32 Budget = CVarRTS_ClientReplication_BudgetPerTick.GetValueOnGameThread();
	int32 MaxActionsPerTick = (World->GetTimeSeconds() < 5.0f) ? FMath::Max(64, Budget) : Budget; // CVAR-driven, with startup burst

	// Throttled reconciliation summary (do not spam every tick)
	{
		static double GLastSummaryTime = 0.0;
		const double NowT = World->GetTimeSeconds();
		const double Interval = 2.0; // seconds
		if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1 && (NowT - GLastSummaryTime) >= Interval)
		{
			GLastSummaryTime = NowT;
			// Compute missing and extra sets relative to authoritative remote (Registry U Bubble)
			TArray<uint32> MissingOnClient;
			TArray<uint32> ExtraOnClient;
			for (uint32 Id : FullRemoteIDs)
			{
				if (!ExistingIDs.Contains(Id) && Id != 0)
				{
					MissingOnClient.Add(Id);
				}
			}
			for (uint32 Id : ExistingIDs)
			{
				if (!FullRemoteIDs.Contains(Id) && Id != 0)
				{
					ExtraOnClient.Add(Id);
				}
			}
			const int32 LiveCount = ExistingIDs.Num();
			const int32 RemoteCount = FullRemoteIDs.Num();
			if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
			{
				auto JoinFew = [](const TArray<uint32>& Arr){ FString S; const int32 Max=FMath::Min(15, Arr.Num()); for (int32 i=0;i<Max;++i){ if (i>0) S+=TEXT(", "); S+=FString::Printf(TEXT("%u"), Arr[i]); } if (Arr.Num()>Max) S+=TEXT(" ..."); return S; };
				UE_LOG(LogTemp, Log, TEXT("ClientReconcileSummary: Live=%d Remote=%d Missing=%d Extra=%d (Missing: %s) (Extra: %s)"),
					LiveCount, RemoteCount, MissingOnClient.Num(), ExtraOnClient.Num(), *JoinFew(MissingOnClient), *JoinFew(ExtraOnClient));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("ClientReconcileSummary: Live=%d Remote=%d Missing=%d Extra=%d"), LiveCount, RemoteCount, MissingOnClient.Num(), ExtraOnClient.Num());
			}
		}
	}
 if (RegistryActor && RegistryActor->Registry.Items.Num() > 0)
	{
		for (const FUnitRegistryItem& It : RegistryActor->Registry.Items)
		{
			if (Actions >= MaxActionsPerTick) { break; }
			if (!ExistingIDs.Contains(It.NetID.GetValue()))
			{
				if (TWeakObjectPtr<AActor>* Found = ByOwnerName.Find(It.OwnerName))
				{
					if (AActor* Act = Found->Get())
					{
						if (UMassActorBindingComponent* Bind = Act->FindComponentByClass<UMassActorBindingComponent>())
						{
       if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
       							{
       								UE_LOG(LogTemp, Warning, TEXT("ClientReconcile: Missing NetID %u for %s -> RequestClientMassLink"), It.NetID.GetValue(), *It.OwnerName.ToString());
       							}
							Bind->RequestClientMassLink();
							Actions++;
						}
					}
				}
				else
				{
					// Shared cached lookup to avoid world-wide scans (guarded by CVAR)
					if (CVarRTS_ClientReplication_EnableCache.GetValueOnGameThread() != 0)
					{
						struct FBindingCache
						{
							TMap<FName, TWeakObjectPtr<UMassActorBindingComponent>> Map;
							double LastBuildTime = -1000.0;
						};
						static TMap<const UWorld*, FBindingCache> GCacheByWorld;
						FBindingCache& Cache = GCacheByWorld.FindOrAdd(World);
						const double NowT = World->GetTimeSeconds();
						const float Interval = FMath::Max(0.1f, CVarRTS_ClientReplication_CacheRebuildSeconds.GetValueOnGameThread());
						if ((NowT - Cache.LastBuildTime) >= Interval)
						{
							Cache.Map.Reset();
							for (TActorIterator<AActor> ItAct(World); ItAct; ++ItAct)
							{
								if (UMassActorBindingComponent* BindC = ItAct->FindComponentByClass<UMassActorBindingComponent>())
								{
									Cache.Map.Add(ItAct->GetFName(), BindC);
								}
							}
							Cache.LastBuildTime = NowT;
						}
						if (TWeakObjectPtr<UMassActorBindingComponent>* FoundBind = Cache.Map.Find(It.OwnerName))
						{
							if (UMassActorBindingComponent* Bind2 = FoundBind->Get())
							{
								const int32 LocalLogLevel = CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread();
								if (LocalLogLevel >= 1)
								{
									UE_LOG(LogTemp, Warning, TEXT("ClientReconcile: Missing NetID %u for %s (cache) -> RequestClientMassLink"), It.NetID.GetValue(), *It.OwnerName.ToString());
								}
								Bind2->RequestClientMassLink();
								Actions++;
							}
						}
					}
				}
			}
		}
	}
	// Unlink any local client entities that are not present in the aggregated remote set
	if (!bRegistryStable)
	{
		if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Verbose, TEXT("ClientReconcile: Skipping unlink this tick (registry not stable yet)"));
		}
	}
	else
	{
		for (uint32 LocalID : ExistingIDs)
		{
			if (Actions >= MaxActionsPerTick) { break; }
			if (!FullRemoteIDs.Contains(LocalID) && LocalID != 0)
			{
				if (TWeakObjectPtr<AActor>* Found = ByNetID.Find(LocalID))
				{
					if (AActor* Act = Found->Get())
					{
						if (UMassActorBindingComponent* Bind = Act->FindComponentByClass<UMassActorBindingComponent>())
						{
       if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
       							{
       								UE_LOG(LogTemp, Warning, TEXT("ClientReconcile: Extra local NetID %u on %s -> RequestClientMassUnlink"), LocalID, *Act->GetName());
       							}
							Bind->RequestClientMassUnlink();
							Actions++;
						}
					}
				}
			}
		}
	}
		
		// If we hit the per-tick action budget, warn so we can correlate with units not moving on client
		if (Actions >= MaxActionsPerTick)
		{
			UE_LOG(LogTemp, Warning, TEXT("ClientReconcile: Budget exhausted this tick (Actions=%d, Max=%d). Some link/unlink may be deferred."), Actions, MaxActionsPerTick);
		}
		
		 EntityQuery.ForEachEntityChunk(Context, [this, AuthoritativeByOwnerName, BubbleByOwnerName, AuthoritativeByUnitIndex, &EntityManager](FMassExecutionContext& Context)
		{
			// Track zero NetID streaks per actor to trigger self-heal retries
			static TMap<TWeakObjectPtr<AActor>, int32> ZeroIdStreak;
			const int32 NumEntities = Context.GetNumEntities();

			TArrayView<FUnitReplicatedTransformFragment> ReplicatedTransformList = Context.GetMutableFragmentView<FUnitReplicatedTransformFragment>();
			TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
			TArrayView<FMassNetworkIDFragment> NetIDList = Context.GetMutableFragmentView<FMassNetworkIDFragment>();
			const TConstArrayView<FMassRepresentationLODFragment> LODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
			TArrayView<FMassAITargetFragment> AITargetList = Context.GetMutableFragmentView<FMassAITargetFragment>();
			// New replicated fragments to apply on client
			TArrayView<FMassCombatStatsFragment> CombatList = Context.GetMutableFragmentView<FMassCombatStatsFragment>();
			TArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
			TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
			TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
			// Prediction fragment view (mutable)
			TArrayView<FMassClientPredictionFragment> PredList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();

			// Log registered NetIDs on client for this chunk (verbose only)
			if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
			{
				const int32 NumIDs = NumEntities;
				const int32 MaxLog = FMath::Min(20, NumIDs);
				FString IdList;
				for (int32 i = 0; i < MaxLog; ++i)
				{
					if (i > 0) { IdList += TEXT(", "); }
					IdList += FString::Printf(TEXT("%u"), NetIDList[i].NetID.GetValue());
				}
				UE_LOG(LogTemp, Verbose, TEXT("ClientReplication: %d entities in chunk. NetIDs[%d]: %s%s"),
					NumIDs, MaxLog, *IdList, (NumIDs > MaxLog ? TEXT(" ...") : TEXT("")));
			}

				// Track bubble items claimed in this chunk to avoid duplicate assignment
				TSet<uint32> ClaimedIDs;
				// Track seen NetIDs in this chunk to detect duplicates
				TSet<uint32> SeenIDs;
			// Build quick lookup for this chunk: NetID -> EntityHandle
			TMap<uint32, FMassEntityHandle> NetToEntity;
			NetToEntity.Reserve(NumEntities);
			for (int32 PreIdx = 0; PreIdx < NumEntities; ++PreIdx)
			{
				const uint32 NID = NetIDList[PreIdx].NetID.GetValue();
				if (NID != 0)
				{
					NetToEntity.Add(NID, Context.GetEntity(PreIdx));
				}
			}
			for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
			{
				// Duplicate NetID detection: ensure per-chunk uniqueness
				const uint32 CurrentID = NetIDList[EntityIdx].NetID.GetValue();

				// 0) Authoritative mapping by UnitIndex first, then OwnerName, then Bubble fallback
				if (AActor* OwnerActor = ActorList[EntityIdx].GetMutable())
				{
					// Try UnitIndex if this is a UnitBase
					int32 OwnerUnitIndex = INDEX_NONE;
					if (const AUnitBase* AsUnit = Cast<AUnitBase>(OwnerActor))
					{
						OwnerUnitIndex = AsUnit->UnitIndex;
					}
					bool bSet = false;
 				if (OwnerUnitIndex != INDEX_NONE)
 				{
 					if (const FMassNetworkID* ByIdx = AuthoritativeByUnitIndex.Find(OwnerUnitIndex))
 					{
       if (NetIDList[EntityIdx].NetID != *ByIdx)
						{
							if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
							{
								UE_LOG(LogTemp, Log, TEXT("ClientAssignNetID: Actor=%s UnitIndex=%d Old=%u New=%u (source=UnitIndex)"),
									ActorList[EntityIdx].GetMutable() ? *ActorList[EntityIdx].GetMutable()->GetName() : TEXT("<none>"), OwnerUnitIndex,
									NetIDList[EntityIdx].NetID.GetValue(), ByIdx->GetValue());
							}
							NetIDList[EntityIdx].NetID = *ByIdx;
						}
						bSet = true;
 					}
 				}
 				if (!bSet)
 				{
 					const FName OwnerName = OwnerActor->GetFName();
 					if (const FMassNetworkID* AuthoritativeId = AuthoritativeByOwnerName.Find(OwnerName))
 					{
       if (NetIDList[EntityIdx].NetID != *AuthoritativeId)
						{
							if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
							{
								UE_LOG(LogTemp, Log, TEXT("ClientAssignNetID: Actor=%s OwnerName=%s Old=%u New=%u (source=OwnerName)"),
									ActorList[EntityIdx].GetMutable() ? *ActorList[EntityIdx].GetMutable()->GetName() : TEXT("<none>"), *OwnerName.ToString(),
									NetIDList[EntityIdx].NetID.GetValue(), AuthoritativeId->GetValue());
							}
							NetIDList[EntityIdx].NetID = *AuthoritativeId;
						}
						bSet = true;
 					}
 					else
 					{
 						if (const FMassNetworkID* BubbleId = BubbleByOwnerName.Find(OwnerName))
 						{
 							if (NetIDList[EntityIdx].NetID != *BubbleId)
 							{
         if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
								{
									UE_LOG(LogTemp, Log, TEXT("ClientAssignNetID: Actor=%s OwnerName=%s Old=%u New=%u (source=BubbleOwnerName)"),
										ActorList[EntityIdx].GetMutable() ? *ActorList[EntityIdx].GetMutable()->GetName() : TEXT("<none>"), *OwnerName.ToString(),
										NetIDList[EntityIdx].NetID.GetValue(), BubbleId->GetValue());
								}
								NetIDList[EntityIdx].NetID = *BubbleId;
 							}
 							bSet = true;
 						}
 					}
 				}
				}
				if (CurrentID != 0)
				{
					if (SeenIDs.Contains(CurrentID))
					{
      if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
      					{
      						UE_LOG(LogTemp, Warning, TEXT("ClientReplication: Duplicate NetID %u detected in chunk"), CurrentID);
      					}
						if (AActor* DupActor = ActorList[EntityIdx].GetMutable())
						{
							if (UMassActorBindingComponent* Bind = DupActor->FindComponentByClass<UMassActorBindingComponent>())
							{
								Bind->RequestClientMassLink();
							}
						}
					}
					else
					{
						SeenIDs.Add(CurrentID);
					}
				}
				// Proximity-based NetID mapping disabled in favor of authoritative registry
				if (LODList[EntityIdx].LOD == EMassLOD::Off)
				{
					continue;
				}
				bool bFromCache = false;
				bool bFromBubble = false;
				bool bActorFallback = false;
				// Update the replicated fragment from cache if available
				FTransform Cached;
				uint32 WantedID_u32 = NetIDList[EntityIdx].NetID.GetValue();

				// NEW: Always apply replicated TagBits and AI Target from the bubble when available (independent of transform path)
				{
					UWorld* WorldForTags = nullptr;
					if (AActor* OwnerA = ActorList[EntityIdx].GetMutable())
					{
						WorldForTags = OwnerA->GetWorld();
					}
					if (WorldForTags)
					{
						if (URTSWorldCacheSubsystem* CacheSub = WorldForTags->GetSubsystem<URTSWorldCacheSubsystem>())
						{
							if (AUnitClientBubbleInfo* Bubble = CacheSub->GetBubble(false))
							{
								const FUnitReplicationItem* TagItem = Bubble->Agents.FindItemByNetID(NetIDList[EntityIdx].NetID);
								if (TagItem)
								{
									if (UMassEntitySubsystem* ES = WorldForTags->GetSubsystem<UMassEntitySubsystem>())
									{
											FMassEntityManager& EMgr = ES->GetMutableEntityManager();
											const FMassEntityHandle EH = Context.GetEntity(EntityIdx);
											ApplyReplicatedTagBits(EMgr, EH, TagItem->TagBits);
											// Apply AI target replication to FMassAITargetFragment regardless of transform source
    							if (AITargetList.IsValidIndex(EntityIdx))
    							{
    								FMassAITargetFragment& AITFrag = AITargetList[EntityIdx];
    								AITFrag.bHasValidTarget = (TagItem->AITargetFlags & 1u) != 0;
    								AITFrag.IsFocusedOnTarget = (TagItem->AITargetFlags & 2u) != 0;
    								AITFrag.LastKnownLocation = FVector(TagItem->AITargetLastKnownLocation);
    								AITFrag.AbilityTargetLocation = FVector(TagItem->AbilityTargetLocation);
    								const uint32 TgtID = TagItem->AITargetNetID;
    								if (TgtID != 0)
    								{
    									if (const FMassEntityHandle* FoundHandle = NetToEntity.Find(TgtID))
    									{
    										AITFrag.TargetEntity = *FoundHandle;
    									}
    								}
    								else
    								{
    									AITFrag.TargetEntity.Reset();
    								}
    								// Rebuild seen sets from replicated NetID arrays
    								AITFrag.PreviouslySeen.Reset();
    								for (const uint32 SeenID : TagItem->AITargetPrevSeenIDs)
    								{
    									if (const FMassEntityHandle* Found = NetToEntity.Find(SeenID))
    									{
    										AITFrag.PreviouslySeen.Add(*Found);
    									}
    								}
    								AITFrag.CurrentlySeen.Reset();
    								for (const uint32 SeenID : TagItem->AITargetCurrSeenIDs)
    								{
    									if (const FMassEntityHandle* Found = NetToEntity.Find(SeenID))
    									{
    										AITFrag.CurrentlySeen.Add(*Found);
    									}
    								}
    							}
    							// Apply replicated Combat/Characteristics/AIState subsets
											if (CombatList.IsValidIndex(EntityIdx))
											{
    								FMassCombatStatsFragment& CS = CombatList[EntityIdx];
    								CS.Health = TagItem->CS_Health;
    								CS.MaxHealth = TagItem->CS_MaxHealth;
    								CS.RunSpeed = TagItem->CS_RunSpeed;
    								CS.TeamId = TagItem->CS_TeamId;
    								// Extended combat stats
    								CS.AttackRange = TagItem->CS_AttackRange;
    								CS.AttackDamage = TagItem->CS_AttackDamage;
    								CS.AttackDuration = TagItem->CS_AttackDuration;
    								CS.IsAttackedDuration = TagItem->CS_IsAttackedDuration;
    								CS.CastTime = TagItem->CS_CastTime;
    								CS.IsInitialized = TagItem->CS_IsInitialized;
    								CS.RotationSpeed = TagItem->CS_RotationSpeed;
    								CS.Armor = TagItem->CS_Armor;
    								CS.MagicResistance = TagItem->CS_MagicResistance;
    								CS.Shield = TagItem->CS_Shield;
    								CS.MaxShield = TagItem->CS_MaxShield;
    								CS.SightRadius = TagItem->CS_SightRadius;
    								CS.LoseSightRadius = TagItem->CS_LoseSightRadius;
    								CS.PauseDuration = TagItem->CS_PauseDuration;
    								CS.bUseProjectile = TagItem->CS_bUseProjectile;
    							}
    							if (CharList.IsValidIndex(EntityIdx))
    							{
    								FMassAgentCharacteristicsFragment& AC = CharList[EntityIdx];
    								AC.bIsFlying = TagItem->AC_bIsFlying;
    								AC.bIsInvisible = TagItem->AC_bIsInvisible;
    								AC.FlyHeight = TagItem->AC_FlyHeight;
    								AC.bCanOnlyAttackFlying = TagItem->AC_bCanOnlyAttackFlying;
    								AC.bCanOnlyAttackGround = TagItem->AC_bCanOnlyAttackGround;
    								AC.bCanBeInvisible = TagItem->AC_bCanBeInvisible;
    								AC.bCanDetectInvisible = TagItem->AC_bCanDetectInvisible;
    								AC.LastGroundLocation = TagItem->AC_LastGroundLocation;
    								AC.DespawnTime = TagItem->AC_DespawnTime;
    								AC.RotatesToMovement = TagItem->AC_RotatesToMovement;
    								AC.RotatesToEnemy = TagItem->AC_RotatesToEnemy;
    								AC.RotationSpeed = TagItem->AC_RotationSpeed;
    								// Rebuild PositionedTransform from quantized pieces
    								const float PPitch = (static_cast<float>(TagItem->AC_PosPitch) / 65535.0f) * 360.0f;
    								const float PYaw   = (static_cast<float>(TagItem->AC_PosYaw)   / 65535.0f) * 360.0f;
    								const float PRoll  = (static_cast<float>(TagItem->AC_PosRoll)  / 65535.0f) * 360.0f;
    								const FQuat PRot   = FQuat(FRotator(PPitch, PYaw, PRoll));
    								AC.PositionedTransform = FTransform(PRot, FVector(TagItem->AC_PosPosition), FVector(TagItem->AC_PosScale));
    								AC.CapsuleHeight = TagItem->AC_CapsuleHeight;
    								AC.CapsuleRadius = TagItem->AC_CapsuleRadius;
    							}
       							if (AIStateList.IsValidIndex(EntityIdx))
							{
								FMassAIStateFragment& AIS = AIStateList[EntityIdx];
								AIS.StateTimer = TagItem->AIS_StateTimer;
								AIS.CanAttack = TagItem->AIS_CanAttack;
								AIS.CanMove = TagItem->AIS_CanMove;
								AIS.HoldPosition = TagItem->AIS_HoldPosition;
								AIS.HasAttacked = TagItem->AIS_HasAttacked;
								AIS.PlaceholderSignal = TagItem->AIS_PlaceholderSignal;
								AIS.StoredLocation = FVector(TagItem->AIS_StoredLocation);
								AIS.SwitchingState = TagItem->AIS_SwitchingState;
								AIS.BirthTime = TagItem->AIS_BirthTime;
								AIS.DeathTime = TagItem->AIS_DeathTime;
								AIS.IsInitialized = TagItem->AIS_IsInitialized;
							}
								// Apply MoveTarget from bubble TagItem early as well to avoid client RPC mirrors
								if (!bStopMovementReplication && MoveTargetList.IsValidIndex(EntityIdx) && TagItem->Move_bHasTarget)
								{
									FMassMoveTargetFragment& MT = MoveTargetList[EntityIdx];
									// Decide if incoming payload is newer than local fragment using ActionID first, then ServerStartTime
									bool bIsNewer = true;
									const uint16 IncomingID = TagItem->Move_ActionID;
									const float IncomingSrvStart = TagItem->Move_ServerStartTime;
									// If this is an initial/legacy payload (ID==0) and the client is actively predicting, do NOT override prediction
									if (IncomingID == 0 && PredList.IsValidIndex(EntityIdx) && PredList[EntityIdx].bHasData)
									{
										bIsNewer = false;
										if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
										{
											UE_LOG(LogTemp, Warning, TEXT("[ClientRep][Move] Skipping initial MoveTarget from replication while predicting (NetID=%u)"), NetIDList[EntityIdx].NetID.GetValue());
										}
									}
									else if (IncomingID != 0)
									{
										const uint16 LocalID = MT.GetCurrentActionID();
										if (IncomingID < LocalID)
										{
											bIsNewer = false;
										}
										else if (IncomingID == LocalID)
										{
											const double LocalSrvStart = MT.GetCurrentActionServerStartTime();
											bIsNewer = (IncomingSrvStart > LocalSrvStart);
										}
									}
					 				if (bIsNewer)
					 				{
					 					MT.Center = FVector(TagItem->Move_Center);
					 					MT.SlackRadius = TagItem->Move_SlackRadius;
					 					MT.DesiredSpeed.Set(TagItem->Move_DesiredSpeed);
					 					MT.IntentAtGoal = static_cast<EMassMovementAction>(TagItem->Move_IntentAtGoal);
					 					MT.DistanceToGoal = TagItem->Move_DistanceToGoal;
					 					// Synchronize action timing so subsequent comparisons are correct
					 					if (AActor* OwnerA2 = ActorList[EntityIdx].GetMutable())
					 					{
					 						UWorld* W = OwnerA2->GetWorld();
					 						const double WorldStart = W ? (double)W->GetTimeSeconds() : 0.0;
					 						MT.CreateReplicatedAction(static_cast<EMassMovementAction>(TagItem->Move_IntentAtGoal), IncomingID, WorldStart, (double)IncomingSrvStart);
					 					}
					 					// Turn off client prediction if a really new MoveTarget arrived
					 					if (PredList.IsValidIndex(EntityIdx))
					 					{
					 						FMassClientPredictionFragment& Pred = PredList[EntityIdx];
					 						if (Pred.bHasData)
					 						{
					 							Pred.bHasData = false;
					 							Pred.Location = FVector::ZeroVector;
					 							Pred.PredDesiredSpeed = 0.f;
					 							Pred.PredAcceptanceRadius = 0.f;
					 						}
					 					}
					 				}
								}
											if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
											{
 											UE_LOG(LogTemp, Log, TEXT("ClientApplyTags: NetID=%u Bits=0x%08x"), NetIDList[EntityIdx].NetID.GetValue(), TagItem->TagBits);
 											UE_LOG(LogTemp, Log, TEXT("ClientBubble AITarget: NetID=%u HasValid=%d Focused=%d LKL=%s AbilityLoc=%s TargetNetID=%u"),
 												NetIDList[EntityIdx].NetID.GetValue(),
 												(TagItem->AITargetFlags & 1u)?1:0,
 												(TagItem->AITargetFlags & 2u)?1:0,
 												*FVector(TagItem->AITargetLastKnownLocation).ToString(),
 												*FVector(TagItem->AbilityTargetLocation).ToString(),
 												TagItem->AITargetNetID);
 											UE_LOG(LogTemp, Log, TEXT("ClientRep Frags: Health=%.1f/%.1f Run=%.1f Team=%d Flying=%d Invis=%d FlyH=%.1f StateT=%.2f CanAtk=%d CanMove=%d Hold=%d"),
 												TagItem->CS_Health, TagItem->CS_MaxHealth, TagItem->CS_RunSpeed, TagItem->CS_TeamId,
 												TagItem->AC_bIsFlying?1:0, TagItem->AC_bIsInvisible?1:0, TagItem->AC_FlyHeight,
 												TagItem->AIS_StateTimer, TagItem->AIS_CanAttack?1:0, TagItem->AIS_CanMove?1:0, TagItem->AIS_HoldPosition?1:0);
											}
									}
								}
							}
						}
					}
				}

				const bool bCacheHit = UnitReplicationCache::GetLatest(NetIDList[EntityIdx].NetID, Cached);
				if (bCacheHit)
				{
					ReplicatedTransformList[EntityIdx].Transform = Cached;
					bFromCache = true;
				}
	const FTransform& RepXf = ReplicatedTransformList[EntityIdx].Transform;
	FTransform FinalXf = RepXf;
				if (RepXf.GetLocation().IsNearlyZero())
				{
					// Try to pull directly from replicated bubble if cache miss
					UWorld* World = nullptr;
					if (AActor* Actor = ActorList[EntityIdx].GetMutable())
					{
						World = Actor->GetWorld();
					}
					if (World)
					{
						AUnitClientBubbleInfo* Bubble = nullptr;
						for (TActorIterator<AUnitClientBubbleInfo> It(World); It; ++It)
						{
							Bubble = *It; break;
						}
						if (Bubble)
						{
							const FMassNetworkID WantedID = NetIDList[EntityIdx].NetID;
							// Try exact match first
							const FUnitReplicationItem* UseItem = Bubble->Agents.FindItemByNetID(WantedID);
							// If we still don't have a NetID (0), assign the nearest unclaimed bubble item
							if (!UseItem && WantedID.GetValue() == 0 && Bubble->Agents.Items.Num() > 0)
							{
								float BestDistSq = TNumericLimits<float>::Max();
								int32 BestIdx = INDEX_NONE;
								FVector RefLoc = FVector::ZeroVector;
								if (AActor* Actor = ActorList[EntityIdx].GetMutable())
								{
									RefLoc = Actor->GetActorLocation();
								}
								for (int32 Idx = 0; Idx < Bubble->Agents.Items.Num(); ++Idx)
								{
									const FUnitReplicationItem& Itm = Bubble->Agents.Items[Idx];
									const uint32 CandID = Itm.NetID.GetValue();
									if (ClaimedIDs.Contains(CandID))
									{
										continue; // already assigned to someone in this chunk this tick
									}
									const float DSq = FVector::DistSquared(RefLoc, FVector(Itm.Location));
									if (DSq < BestDistSq)
									{
										BestDistSq = DSq;
										BestIdx = Idx;
									}
								}
								if (BestIdx != INDEX_NONE)
								{
									UseItem = &Bubble->Agents.Items[BestIdx];
 								// Do not patch NetID on client; wait for replication
 								ClaimedIDs.Add(UseItem->NetID.GetValue());
								}
							}
							if (UseItem)
							{
								// If we still have NetID 0, adopt the bubble's NetID (exact or nearest mapping)
								if (NetIDList[EntityIdx].NetID.GetValue() == 0)
								{
									NetIDList[EntityIdx].NetID = UseItem->NetID;
								}
								// Dequantize rotation and build transform
								const float Pitch = (static_cast<float>(UseItem->PitchQuantized) / 65535.0f) * 360.0f;
								const float Yaw   = (static_cast<float>(UseItem->YawQuantized)   / 65535.0f) * 360.0f;
								const float Roll  = (static_cast<float>(UseItem->RollQuantized)  / 65535.0f) * 360.0f;
								FinalXf = FTransform(FQuat(FRotator(Pitch, Yaw, Roll)), FVector(UseItem->Location), FVector(UseItem->Scale));
								bFromBubble = true;
   							// Apply replicated tag bits to this entity on the client
							if (UMassEntitySubsystem* ES = World->GetSubsystem<UMassEntitySubsystem>())
							{
								FMassEntityManager& EMgr = ES->GetMutableEntityManager();
								const FMassEntityHandle EH = Context.GetEntity(EntityIdx);
								ApplyReplicatedTagBits(EMgr, EH, UseItem->TagBits);
								// Apply AI target replication to FMassAITargetFragment
								if (AITargetList.IsValidIndex(EntityIdx))
								{
									FMassAITargetFragment& AITFrag = AITargetList[EntityIdx];
									AITFrag.bHasValidTarget = (UseItem->AITargetFlags & 1u) != 0;
									AITFrag.IsFocusedOnTarget = (UseItem->AITargetFlags & 2u) != 0;
									AITFrag.LastKnownLocation = FVector(UseItem->AITargetLastKnownLocation);
									AITFrag.AbilityTargetLocation = FVector(UseItem->AbilityTargetLocation);
									// Resolve target entity handle on client if we know the NetID -> Entity mapping in this chunk
									const uint32 TgtID = UseItem->AITargetNetID;
									if (TgtID != 0)
									{
										if (const FMassEntityHandle* FoundHandle = NetToEntity.Find(TgtID))
										{
											AITFrag.TargetEntity = *FoundHandle;
										}
									}
									else
									{
										AITFrag.TargetEntity.Reset();
									}
								}
								// Also apply replicated CombatStats/Characteristics/AIState from bubble item to ensure full data on client
 							if (CombatList.IsValidIndex(EntityIdx))
 							{
 								FMassCombatStatsFragment& CS = CombatList[EntityIdx];
 								CS.Health = UseItem->CS_Health;
 								CS.MaxHealth = UseItem->CS_MaxHealth;
 								CS.RunSpeed = UseItem->CS_RunSpeed;
 								CS.TeamId = UseItem->CS_TeamId;
 								CS.AttackRange = UseItem->CS_AttackRange;
 								CS.AttackDamage = UseItem->CS_AttackDamage;
 								CS.AttackDuration = UseItem->CS_AttackDuration;
 								CS.IsAttackedDuration = UseItem->CS_IsAttackedDuration;
 								CS.CastTime = UseItem->CS_CastTime;
 								CS.IsInitialized = UseItem->CS_IsInitialized;
 								CS.RotationSpeed = UseItem->CS_RotationSpeed;
 								CS.Armor = UseItem->CS_Armor;
 								CS.MagicResistance = UseItem->CS_MagicResistance;
 								CS.Shield = UseItem->CS_Shield;
 								CS.MaxShield = UseItem->CS_MaxShield;
 								CS.SightRadius = UseItem->CS_SightRadius;
 								CS.LoseSightRadius = UseItem->CS_LoseSightRadius;
 								CS.PauseDuration = UseItem->CS_PauseDuration;
 								CS.bUseProjectile = UseItem->CS_bUseProjectile;
 							}
 							if (CharList.IsValidIndex(EntityIdx))
 							{
 								FMassAgentCharacteristicsFragment& AC = CharList[EntityIdx];
 								AC.bIsFlying = UseItem->AC_bIsFlying;
 								AC.bIsInvisible = UseItem->AC_bIsInvisible;
 								AC.FlyHeight = UseItem->AC_FlyHeight;
 								AC.bCanOnlyAttackFlying = UseItem->AC_bCanOnlyAttackFlying;
 								AC.bCanOnlyAttackGround = UseItem->AC_bCanOnlyAttackGround;
 								AC.bCanBeInvisible = UseItem->AC_bCanBeInvisible;
 								AC.bCanDetectInvisible = UseItem->AC_bCanDetectInvisible;
 								AC.LastGroundLocation = UseItem->AC_LastGroundLocation;
 								AC.DespawnTime = UseItem->AC_DespawnTime;
 								AC.RotatesToMovement = UseItem->AC_RotatesToMovement;
 								AC.RotatesToEnemy = UseItem->AC_RotatesToEnemy;
 								AC.RotationSpeed = UseItem->AC_RotationSpeed;
 								// Rebuild PositionedTransform from quantized pieces
 								const float PPitch = (static_cast<float>(UseItem->AC_PosPitch) / 65535.0f) * 360.0f;
 								const float PYaw   = (static_cast<float>(UseItem->AC_PosYaw)   / 65535.0f) * 360.0f;
 								const float PRoll  = (static_cast<float>(UseItem->AC_PosRoll)  / 65535.0f) * 360.0f;
 								const FQuat PRot   = FQuat(FRotator(PPitch, PYaw, PRoll));
 								AC.PositionedTransform = FTransform(PRot, FVector(UseItem->AC_PosPosition), FVector(UseItem->AC_PosScale));
 								AC.CapsuleHeight = UseItem->AC_CapsuleHeight;
 								AC.CapsuleRadius = UseItem->AC_CapsuleRadius;
 							}
 							if (AIStateList.IsValidIndex(EntityIdx))
 							{
 								FMassAIStateFragment& AIS = AIStateList[EntityIdx];
 								AIS.StateTimer = UseItem->AIS_StateTimer;
 								AIS.CanAttack = UseItem->AIS_CanAttack;
 								AIS.CanMove = UseItem->AIS_CanMove;
 								AIS.HoldPosition = UseItem->AIS_HoldPosition;
 								AIS.HasAttacked = UseItem->AIS_HasAttacked;
 								AIS.PlaceholderSignal = UseItem->AIS_PlaceholderSignal;
 								AIS.StoredLocation = FVector(UseItem->AIS_StoredLocation);
 								AIS.SwitchingState = UseItem->AIS_SwitchingState;
 								AIS.BirthTime = UseItem->AIS_BirthTime;
 								AIS.DeathTime = UseItem->AIS_DeathTime;
 								AIS.IsInitialized = UseItem->AIS_IsInitialized;
 							}
								// Apply MoveTarget if present (guarded by bStopMovementReplication)
		           if (UseItem->Move_bHasTarget)
	       {
	           if (bStopMovementReplication)
	           {
	               if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
	               {
	                   UE_LOG(LogTemp, Verbose, TEXT("[ClientRep] Skipping MoveTarget replication (UseItem path) for NetID=%u"), NetIDList[EntityIdx].NetID.GetValue());
	               }
	           }
	           else if (MoveTargetList.IsValidIndex(EntityIdx))
	           {
	               FMassMoveTargetFragment& MT = MoveTargetList[EntityIdx];
	               // Decide newer vs older by Move_ActionID then Move_ServerStartTime
	               bool bIsNewer2 = true;
	               const uint16 IncomingID2 = UseItem->Move_ActionID;
	               const float IncomingSrvStart2 = UseItem->Move_ServerStartTime;
	               // If initial/legacy payload (ID==0) and client is currently predicting, keep prediction and skip overriding
	               if (IncomingID2 == 0 && PredList.IsValidIndex(EntityIdx) && PredList[EntityIdx].bHasData)
	               {
	                   bIsNewer2 = false;
	                   if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
	                   {
	                       UE_LOG(LogTemp, Warning, TEXT("[ClientRep][Move] Skipping initial MoveTarget (UseItem path) while predicting (NetID=%u)"), NetIDList[EntityIdx].NetID.GetValue());
	                   }
	               }
	               else if (IncomingID2 != 0)
	               {
	                   const uint16 LocalID2 = MT.GetCurrentActionID();
	                   if (IncomingID2 < LocalID2)
	                   {
	                       bIsNewer2 = false;
	                   }
	                   else if (IncomingID2 == LocalID2)
	                   {
	                       const double LocalSrvStart2 = MT.GetCurrentActionServerStartTime();
	                       bIsNewer2 = (IncomingSrvStart2 > LocalSrvStart2);
	                   }
	               }
	               if (bIsNewer2)
	               {
	                   MT.Center = FVector(UseItem->Move_Center);
	                   MT.SlackRadius = UseItem->Move_SlackRadius;
	                   MT.DesiredSpeed.Set(UseItem->Move_DesiredSpeed);
	                   MT.IntentAtGoal = static_cast<EMassMovementAction>(UseItem->Move_IntentAtGoal);
	                   MT.DistanceToGoal = UseItem->Move_DistanceToGoal;
	                   if (AActor* OwnerA3 = ActorList[EntityIdx].GetMutable())
	                   {
	                       UWorld* W3 = OwnerA3->GetWorld();
	                       const double WorldStart3 = W3 ? (double)W3->GetTimeSeconds() : 0.0;
	                       MT.CreateReplicatedAction(static_cast<EMassMovementAction>(UseItem->Move_IntentAtGoal), IncomingID2, WorldStart3, (double)IncomingSrvStart2);
	                   }
	                   // Turn off client prediction if a really new MoveTarget arrived
	                   if (PredList.IsValidIndex(EntityIdx))
	                   {
	                       FMassClientPredictionFragment& Pred = PredList[EntityIdx];
	                       if (Pred.bHasData)
	                       {
	                           Pred.bHasData = false;
	                           Pred.Location = FVector::ZeroVector;
	                           Pred.PredDesiredSpeed = 0.f;
	                           Pred.PredAcceptanceRadius = 0.f;
	                       }
	                   }
	               }
	           }
	           else
	           {
	               // Unconditional warning on client: expected FMassMoveTargetFragment but it's missing for this entity
	               static TSet<uint32> WarnedOnceClient; // avoid spamming per NetID
	               const uint32 IdVal = NetIDList[EntityIdx].NetID.GetValue();
	               if (!WarnedOnceClient.Contains(IdVal))
	               {
	                   WarnedOnceClient.Add(IdVal);
	                   const FName OwnerN = (ActorList.IsValidIndex(EntityIdx) && ActorList[EntityIdx].GetMutable()) ? ActorList[EntityIdx].GetMutable()->GetFName() : NAME_None;
	                   UE_LOG(LogTemp, Warning, TEXT("[ClientRep] Missing FMassMoveTargetFragment for Entity NetID=%u Owner=%s (has Move target in payload)"), IdVal, *OwnerN.ToString());
	               }
	           }
	       }
							}
						}
						}
					}
				
					// If still zero, use actor fallback to avoid popping to origin
					if (FinalXf.GetLocation().IsNearlyZero())
					{
						if (AActor* Actor = ActorList[EntityIdx].GetMutable())
						{
							FinalXf = Actor->GetActorTransform();
							bActorFallback = true;
						}
					}
				}
				// Replication mode: either full replication (direct set) or reconciliation via steering/force
				if (bUseFullReplication)
				{
					// Disable reconciliation: directly set the Mass transform to authoritative
					FTransform& ClientXf = TransformList[EntityIdx].GetMutableTransform();
					ClientXf = FinalXf;
					// Also zero out steering/force to prevent drift from movement systems this tick
					TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
					TArrayView<FMassSteeringFragment> SteeringList = Context.GetMutableFragmentView<FMassSteeringFragment>();
					if (ForceList.IsValidIndex(EntityIdx))
					{
						ForceList[EntityIdx].Value = FVector::ZeroVector;
					}
					if (SteeringList.IsValidIndex(EntityIdx))
					{
						SteeringList[EntityIdx].DesiredVelocity = FVector::ZeroVector;
					}
				}
				else
				{
					// Skip reconciliation entirely while client-side prediction is active
					bool bPredicting = false;
					if (PredList.Num() > 0)
					{
						bPredicting = PredList[EntityIdx].bHasData;
					}
					if (bPredicting)
					{
						if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
						{
							UE_LOG(LogTemp, Warning, TEXT("ClientReconcile: Skipped (predicting) NetID=%u"), NetIDList[EntityIdx].NetID.GetValue());
						}
						continue;
					}
					// Server reconciliation: compare authoritative FinalXf vs current Mass transform and apply gentle correction via Force/Steering
					FTransform& ClientXf = TransformList[EntityIdx].GetMutableTransform();
					const FVector CurrentLoc = ClientXf.GetLocation();
					const FVector TargetLoc = FinalXf.GetLocation();
					FVector PosError = TargetLoc - CurrentLoc;
					// Only correct horizontal to avoid oscillations in height; vertical handled by other processors
					FVector PosErrorXY(PosError.X, PosError.Y, 0.f);
					const float ErrorDistSq = PosErrorXY.SizeSquared();
					// One-time snap to server if overall error exceeds FullReplicationDistance and we're in reconciliation mode
					{
						static TSet<uint32> GSnappedOnce;
						const uint32 ThisNetID = NetIDList[EntityIdx].NetID.GetValue();
						const float Dist3DSq = FVector::DistSquared(TargetLoc, CurrentLoc);
						const float SnapThreshSq = FullReplicationDistance > 0.f ? FMath::Square(FullReplicationDistance) : 0.f;
						// If far away and haven't snapped yet, do a single full replication (snap) this tick
						if (SnapThreshSq > 0.f && Dist3DSq > SnapThreshSq && !GSnappedOnce.Contains(ThisNetID))
						{
							FTransform& ClientXfSnap = TransformList[EntityIdx].GetMutableTransform();
							ClientXfSnap = FinalXf;
							// Zero force/steering this tick to avoid overshoot
							TArrayView<FMassForceFragment> ForceListSnap = Context.GetMutableFragmentView<FMassForceFragment>();
							TArrayView<FMassSteeringFragment> SteeringListSnap = Context.GetMutableFragmentView<FMassSteeringFragment>();
							if (ForceListSnap.IsValidIndex(EntityIdx)) { ForceListSnap[EntityIdx].Value = FVector::ZeroVector; }
							if (SteeringListSnap.IsValidIndex(EntityIdx)) { SteeringListSnap[EntityIdx].DesiredVelocity = FVector::ZeroVector; }
							GSnappedOnce.Add(ThisNetID);
							if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
							{
								UE_LOG(LogTemp, Warning, TEXT("ClientSnapOnce: NetID=%u Dist=%.1f (>%.1f)"), ThisNetID, FMath::Sqrt(Dist3DSq), FullReplicationDistance);
							}
							// Skip further reconciliation for this entity this tick
							continue;
						}
						// If we've snapped previously and are now within half the threshold, allow future snaps again
						if (GSnappedOnce.Contains(ThisNetID) && Dist3DSq <= FMath::Square(FMath::Max(FullReplicationDistance * 0.5f, 100.f)))
						{
							GSnappedOnce.Remove(ThisNetID);
						}
					}
					// Thresholds and gains
				
					if (ErrorDistSq > MinErrorForCorrectionSq)
					{
						// Convert position error into a corrective acceleration vector
						FVector Corrective = PosErrorXY.GetClampedToMaxSize(MaxCorrectionAccel / FMath::Max(1.f, Kp)) * Kp;
						// Apply as additional force (accel) this frame; UnitApplyMassMovementProcessor consumes Force.Value
						TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
						TArrayView<FMassSteeringFragment> SteeringList = Context.GetMutableFragmentView<FMassSteeringFragment>();
						if (ForceList.IsValidIndex(EntityIdx))
						{
							FMassForceFragment& ForceFrag = ForceList[EntityIdx];
							ForceFrag.Value += Corrective;
						}
						// Also bias steering slightly toward the authoritative target to converge faster
						if (SteeringList.IsValidIndex(EntityIdx))
						{
							FMassSteeringFragment& Steer = SteeringList[EntityIdx];
							Steer.DesiredVelocity += Corrective * 0.1f; // small bias
						}
						// Diagnostic: log corrective force magnitude and error distance (warning level)
						if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 1)
						{
							const float Err = FMath::Sqrt(ErrorDistSq);
							UE_LOG(LogTemp, Warning, TEXT("ClientReconcileForce: NetID=%u ErrXY=%.1f Corr=(%.1f,%.1f,%.1f) Kp=%.1f MaxAccel=%.1f"),
								NetIDList[EntityIdx].NetID.GetValue(), Err, Corrective.X, Corrective.Y, Corrective.Z, Kp, MaxCorrectionAccel);
						}
					}
				}

    // Detailed client log: what transform we compared and source (disabled by default)
				if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
				{
					const AActor* OA = ActorList[EntityIdx].GetMutable();
					const int32 UnitIndex = (OA && OA->IsA<AUnitBase>()) ? static_cast<const AUnitBase*>(OA)->UnitIndex : INDEX_NONE;
					UE_LOG(LogTemp, Log, TEXT("ClientApplyXf: Actor=%s UnitIndex=%d NetID=%u Src=%s Loc=(%.1f,%.1f,%.1f) Rot=(P%.1f Y%.1f R%.1f) Scale=(%.2f,%.2f,%.2f)"),
						OA ? *OA->GetName() : TEXT("<none>"), UnitIndex, NetIDList[EntityIdx].NetID.GetValue(),
						bFromCache ? TEXT("Cache") : (bFromBubble ? TEXT("Bubble") : (bActorFallback ? TEXT("ActorFallback") : TEXT("ReplicatedFrag"))),
						FinalXf.GetLocation().X, FinalXf.GetLocation().Y, FinalXf.GetLocation().Z,
						FinalXf.Rotator().Pitch, FinalXf.Rotator().Yaw, FinalXf.Rotator().Roll,
						FinalXf.GetScale3D().X, FinalXf.GetScale3D().Y, FinalXf.GetScale3D().Z);
				}
				// Additional verbose diagnostics: log AI target fragment values for this entity on the client
				if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2 && AITargetList.IsValidIndex(EntityIdx))
				{
					const FMassAITargetFragment& AIT = AITargetList[EntityIdx];
					uint32 TargetNetIDDiag = 0u;
					if (AIT.TargetEntity.IsSet() && EntityManager.IsEntityValid(AIT.TargetEntity))
					{
						if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT.TargetEntity))
						{
							TargetNetIDDiag = TgtNet->NetID.GetValue();
						}
					}
					UE_LOG(LogTemp, Log, TEXT("ClientAITarget: Actor=%s NetID=%u HasValid=%d Focused=%d LKL=%s AbilityLoc=%s TargetSet=%d TargetNetID=%u"),
						ActorList[EntityIdx].GetMutable() ? *ActorList[EntityIdx].GetMutable()->GetName() : TEXT("<none>"),
						NetIDList[EntityIdx].NetID.GetValue(),
						AIT.bHasValidTarget?1:0,
						AIT.IsFocusedOnTarget?1:0,
						*AIT.LastKnownLocation.ToString(),
						*AIT.AbilityTargetLocation.ToString(),
						AIT.TargetEntity.IsSet()?1:0,
						TargetNetIDDiag);
				}

    // Self-heal if NetID repeatedly missing on client
				AActor* OwnerActor2 = ActorList[EntityIdx].GetMutable();
				if (OwnerActor2)
				{
					int32& Streak = ZeroIdStreak.FindOrAdd(OwnerActor2);
					if (NetIDList[EntityIdx].NetID.GetValue() == 0)
					{
						Streak++;
						if (Streak >= 3) // three consecutive ticks
						{
							if (UMassActorBindingComponent* Bind = OwnerActor2->FindComponentByClass<UMassActorBindingComponent>())
							{
								Bind->RequestClientMassLink();
							}
						}
					}
					else
					{
						ZeroIdStreak.Remove(OwnerActor2);
					}
				}
			}

		});
}