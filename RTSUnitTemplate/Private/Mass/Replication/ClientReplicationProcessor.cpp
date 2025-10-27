// Fill out your copyright notice in the Description page of Project Settings.


#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif

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
    16,
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
// Toggle between full replication and reconciliation (0 = reconciliation, 1 = full replication)
static TAutoConsoleVariable<int32> CVarRTS_ClientReplication_FullReplication(
    TEXT("net.RTS.ClientReplication.FullReplication"),
    1,
    TEXT("1 = Full transform replication (disable steering/force reconciliation). 0 = Reconciliation via steering/force."),
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
	EntityQuery.RegisterWithProcessor(*this);
	
}

void UClientReplicationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (bSkipReplication) return;
	
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
		
 EntityQuery.ForEachEntityChunk(Context, [this, AuthoritativeByOwnerName, BubbleByOwnerName, AuthoritativeByUnitIndex](FMassExecutionContext& Context)
		{
			// Track zero NetID streaks per actor to trigger self-heal retries
			static TMap<TWeakObjectPtr<AActor>, int32> ZeroIdStreak;
			const int32 NumEntities = Context.GetNumEntities();

			TArrayView<FUnitReplicatedTransformFragment> ReplicatedTransformList = Context.GetMutableFragmentView<FUnitReplicatedTransformFragment>();
			TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
			TArrayView<FMassNetworkIDFragment> NetIDList = Context.GetMutableFragmentView<FMassNetworkIDFragment>();
			const TConstArrayView<FMassRepresentationLODFragment> LODList = Context.GetFragmentView<FMassRepresentationLODFragment>();

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

				// NEW: Always apply replicated TagBits from the bubble when available (independent of transform path)
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
										if (CVarRTS_ClientReplication_LogLevel.GetValueOnGameThread() >= 2)
										{
											UE_LOG(LogTemp, Verbose, TEXT("ClientApplyTags: NetID=%u Bits=0x%08x"), NetIDList[EntityIdx].NetID.GetValue(), TagItem->TagBits);
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
					// Server reconciliation: compare authoritative FinalXf vs current Mass transform and apply gentle correction via Force/Steering
					FTransform& ClientXf = TransformList[EntityIdx].GetMutableTransform();
					const FVector CurrentLoc = ClientXf.GetLocation();
					const FVector TargetLoc = FinalXf.GetLocation();
					FVector PosError = TargetLoc - CurrentLoc;
					// Only correct horizontal to avoid oscillations in height; vertical handled by other processors
					FVector PosErrorXY(PosError.X, PosError.Y, 0.f);
					const float ErrorDistSq = PosErrorXY.SizeSquared();
					// Thresholds and gains
					static const float MinErrorForCorrectionSq = FMath::Square(20.f); // 5 cm
					static const float MaxCorrectionAccel = 3000.f; // cm/s^2
					static const float Kp = 6.0f; // proportional gain to turn error into desired velocity/accel
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