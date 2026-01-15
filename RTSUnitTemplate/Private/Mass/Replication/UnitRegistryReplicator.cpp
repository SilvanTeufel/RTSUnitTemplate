
/*
#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif
*/
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/MassActorBindingComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "TimerManager.h"
#include "Mass/Replication/UnitReplicationCacheSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassReplicationFragments.h"
#include "HAL/IConsoleManager.h"

// 0=Off, 1=Warn, 2=Verbose
static TAutoConsoleVariable<int32> CVarRTS_Registry_LogLevel(
	TEXT("net.RTS.Registry.LogLevel"),
	0,
	TEXT("Logging level for UnitRegistryReplicator diagnostics: 0=Off, 1=Warn, 2=Verbose."),
	ECVF_Default);

// Grace period before client starts destroying unregistered units (allows time for registration to complete)
static TAutoConsoleVariable<float> CVarRTS_Registry_ClientReconcileGracePeriod(
	TEXT("net.RTS.Registry.ClientReconcileGracePeriod"),
	15.0f,
	TEXT("Seconds after world start before client starts destroying unregistered units. Default 15s to allow full registration."),
	ECVF_Default);

// Diagnostics timer interval - faster during startup, slower after
static TAutoConsoleVariable<float> CVarRTS_Registry_DiagnosticsInterval(
	TEXT("net.RTS.Registry.DiagnosticsInterval"),
	1.0f,
	TEXT("Seconds between server diagnostics ticks. Default 1s for faster startup registration."),
	ECVF_Default);

namespace { inline int32 RegLogLevel(){ return CVarRTS_Registry_LogLevel.GetValueOnGameThread(); } }

namespace { constexpr bool GRegistryImportantLogs = false; }

AUnitRegistryReplicator::AUnitRegistryReplicator()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
	Registry.OwnerActor = this;
}

void AUnitRegistryReplicator::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_Client)
	{
		ResetNetIDCounter();
		// Start periodic diagnostics on server to detect unregistered/stale units
		// Use CVAR-controlled interval (default 1s) for faster startup registration
		if (UWorld* W = GetWorld())
		{
			const float DiagInterval = FMath::Max(0.5f, CVarRTS_Registry_DiagnosticsInterval.GetValueOnGameThread());
			W->GetTimerManager().SetTimer(DiagnosticsTimerHandle, this, &AUnitRegistryReplicator::ServerDiagnosticsTick, DiagInterval, true, 0.5f);
		}
		if (RegLogLevel() >= 1)
		{
			//UE_LOG(LogTemp, Log, TEXT("[RTS.Registry] Reset NetID counter to 1 (world=%s)"), *GetWorld()->GetName());
		}
	}
}

void AUnitRegistryReplicator::ResetNetIDCounter()
{
	NextNetID = 1;
	QuarantinedNetIDs.Empty();
}

uint32 AUnitRegistryReplicator::GetNextNetID()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	
	// Skip any IDs that are currently in quarantine
	while (true)
	{
		const uint32 TrialID = NextNetID++;
		
		double* Expiration = QuarantinedNetIDs.Find(TrialID);
		if (Expiration)
		{
			if (Now < *Expiration)
			{
				// Still quarantined, skip it
				continue;
			}
			else
			{
				// Expired, we can use it (and remove from map)
				QuarantinedNetIDs.Remove(TrialID);
				return TrialID;
			}
		}
		
		// Not in quarantine, use it
		return TrialID;
	}
}

void AUnitRegistryReplicator::QuarantineNetID(uint32 NetID)
{
	if (NetID == 0) return;
	
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	QuarantinedNetIDs.Add(NetID, Now + static_cast<double>(NetIDQuarantineTime));
	
	if (RegLogLevel() >= 2)
	{
		//UE_LOG(LogTemp, Log, TEXT("[RTS.Registry] Quarantined NetID %u for %.1fs"), NetID, NetIDQuarantineTime);
	}
}

void AUnitRegistryReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUnitRegistryReplicator, Registry);
}

void AUnitRegistryReplicator::OnRep_Registry()
{
	Registry.OwnerActor = this;
	// Track client-side update timing to help debounce reconciliation
	if (GetNetMode() == NM_Client)
	{
		ClientOnRepCounter++;
		if (UWorld* W = GetWorld())
		{
			ClientLastOnRepTime = W->GetTimeSeconds();
		}
	}
	// Client-side: run diagnostics comparing world units to registry mapping
	#if !UE_SERVER
	if (UWorld* World = GetWorld())
	{
		// Run diagnostics after registry update
		ServerDiagnosticsTick(); // safe call on client - function will early return if NM_Client

		// Client-side reconcile: destroy zombie actors that are not in registry or have invalid Mass binding
		// BUT only after the grace period has passed to allow time for registration to complete
		if (World->GetNetMode() == NM_Client)
		{
			const float WorldTime = World->GetTimeSeconds();
			const float GracePeriod = CVarRTS_Registry_ClientReconcileGracePeriod.GetValueOnGameThread();
			const bool bInGracePeriod = (WorldTime < GracePeriod);
			
			// During grace period, only log warnings but don't destroy units
			// This prevents destroying units that are still being registered
			if (bInGracePeriod)
			{
				if (RegLogLevel() >= 2)
				{
					UE_LOG(LogTemp, Verbose, TEXT("[RTS.Registry] Client in grace period (%.1fs < %.1fs), skipping zombie cleanup."), WorldTime, GracePeriod);
				}
			}
			else
			{
				// Build quick sets for lookup
				TSet<int32> RegIndices;
				TSet<FName> RegOwners;
				RegIndices.Reserve(Registry.Items.Num());
				RegOwners.Reserve(Registry.Items.Num());
				for (const FUnitRegistryItem& It : Registry.Items)
				{
					if (It.UnitIndex != INDEX_NONE) { RegIndices.Add(It.UnitIndex); }
					if (It.OwnerName != NAME_None) { RegOwners.Add(It.OwnerName); }
				}
				int32 Cleaned = 0;
				for (TActorIterator<AUnitBase> It(World); It; ++It)
				{
					AUnitBase* Unit = *It;
					if (!IsValid(Unit)) { continue; }
					// Do NOT destroy units simply because they are Dead; keep them until actor/entity despawns.
					const bool bInReg = (Unit->UnitIndex != INDEX_NONE && RegIndices.Contains(Unit->UnitIndex)) || RegOwners.Contains(Unit->GetFName());
					bool bHasValidBinding = false;
					if (UMassActorBindingComponent* Bind = Unit->FindComponentByClass<UMassActorBindingComponent>())
					{
						bHasValidBinding = Bind->GetEntityHandle().IsSet();
					}
					if (!bInReg || !bHasValidBinding)
					{
						Unit->Destroy();
						++Cleaned;
					}
				}
				if (Cleaned > 0)
				{
					if (RegLogLevel() >= 1)
					{
						//UE_LOG(LogTemp, Warning, TEXT("[RTS.Registry] Client reconcile destroyed %d zombie Units after registry update."), Cleaned);
					}
				}
			}

			// Trim client-side transform cache entries whose NetIDs are no longer present in the registry
			{
				TSet<FMassNetworkID> ValidIDs;
				ValidIDs.Reserve(Registry.Items.Num());
				for (const FUnitRegistryItem& It : Registry.Items)
				{
					ValidIDs.Add(It.NetID);
				}
				int32 Trimmed = 0;
				TArray<FMassNetworkID> KeysToCheck;
				UnitReplicationCache::Map().GenerateKeyArray(KeysToCheck);
				for (const FMassNetworkID& KeyID : KeysToCheck)
				{
					if (!ValidIDs.Contains(KeyID))
					{
						UnitReplicationCache::Remove(KeyID);
						++Trimmed;
					}
				}
				if (Trimmed > 0)
				{
					if (RegLogLevel() >= 2)
					{
						UE_LOG(LogTemp, Verbose, TEXT("[RTS.Replication] Client trimmed %d stale NetIDs from UnitReplicationCache after registry update."), Trimmed);
					}
				}
			}
		}
	}
	#endif
}

// Helper to stringify NetID
static FString NetIDToStr(const FMassNetworkID& ID)
{
	return FString::Printf(TEXT("%u"), ID.GetValue());
}

// Core diagnostics: compare in-world Units vs Registry entries and log discrepancies
static void RunDiagnosticsForWorld(UWorld& World, const FUnitRegistryArray& Registry, const TCHAR* Context)
{
	// Build lookup of registry by UnitIndex and OwnerName
	TMap<int32, const FUnitRegistryItem*> RegByIndex;
	TMap<FName, const FUnitRegistryItem*> RegByOwner;
	RegByIndex.Reserve(Registry.Items.Num());
	RegByOwner.Reserve(Registry.Items.Num());
	for (const FUnitRegistryItem& It : Registry.Items)
	{
		if (It.UnitIndex != INDEX_NONE)
		{
			RegByIndex.Add(It.UnitIndex, &It);
		}
		if (It.OwnerName != NAME_None)
		{
			RegByOwner.Add(It.OwnerName, &It);
		}
	}

	// Collect live units in world
	TSet<int32> LiveIndices;
	TSet<FName> LiveOwners;
	int32 LiveUnits = 0;
 for (TActorIterator<AUnitBase> It(&World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsValid(Unit)) continue;
		// Ignore units that are dead; they should not be in the registry and shouldn't count as live
		LiveUnits++;
		LiveOwners.Add(Unit->GetFName());
		LiveIndices.Add(Unit->UnitIndex);
	}

	// Detect unregistered live units (by UnitIndex primarily)
	TArray<int32> MissingByIndex;
	for (int32 Idx : LiveIndices)
	{
		if (Idx == INDEX_NONE) continue;
		if (!RegByIndex.Contains(Idx))
		{
			MissingByIndex.Add(Idx);
		}
	}
	// Fallback: by OwnerName
	TArray<FName> MissingByOwner;
	for (const FName& Name : LiveOwners)
	{
		if (!RegByOwner.Contains(Name))
		{
			MissingByOwner.Add(Name);
		}
	}

	// Detect stale registry entries that no longer exist in world
	TArray<int32> StaleByIndex;
	TArray<FName> StaleByOwner;
	for (const auto& KVP : RegByIndex)
	{
		if (!LiveIndices.Contains(KVP.Key))
		{
			StaleByIndex.Add(KVP.Key);
		}
	}
	for (const auto& KVP : RegByOwner)
	{
		if (!LiveOwners.Contains(KVP.Key))
		{
			StaleByOwner.Add(KVP.Key);
		}
	}

	const bool bHasIssues = MissingByIndex.Num() > 0 || MissingByOwner.Num() > 0 || StaleByIndex.Num() > 0 || StaleByOwner.Num() > 0;
	const int32 RegCount = Registry.Items.Num();
	const ENetMode Mode = World.GetNetMode();
	const TCHAR* ModeStr = (Mode == NM_DedicatedServer) ? TEXT("Server") : (Mode == NM_ListenServer ? TEXT("ListenServer") : (Mode == NM_Client ? TEXT("Client") : TEXT("Standalone")));
	if (bHasIssues)
	{
		auto JoinInts = [](const TArray<int32>& Arr){ FString S; for (int32 i = 0; i < Arr.Num() && i < 20; ++i){ if (i>0) S += TEXT(", "); S += FString::FromInt(Arr[i]); } if (Arr.Num()>20) S += TEXT(", ..."); return S; };
		auto JoinNames = [](const TArray<FName>& Arr){ FString S; for (int32 i = 0; i < Arr.Num() && i < 20; ++i){ if (i>0) S += TEXT(", "); S += Arr[i].ToString(); } if (Arr.Num()>20) S += TEXT(", ..."); return S; };

		if (RegLogLevel() >= 1)
		{
			/*
			UE_LOG(LogTemp, Warning, TEXT("[RTS.Replication] DIAG[%s|%s]: LiveUnits=%d, Registry=%d, Missing(Index)=%d, Missing(Owner)=%d, Stale(Index)=%d, Stale(Owner)=%d"),
				Context, ModeStr, LiveUnits, RegCount, MissingByIndex.Num(), MissingByOwner.Num(), StaleByIndex.Num(), StaleByOwner.Num());
			*/
			if (RegLogLevel() >= 2)
			{
				if (MissingByIndex.Num() > 0)
				{
					//UE_LOG(LogTemp, Warning, TEXT("  Missing UnitIndex -> Not in Registry: [%s]"), *JoinInts(MissingByIndex));
				}
				if (MissingByOwner.Num() > 0)
				{
					//UE_LOG(LogTemp, Warning, TEXT("  Missing OwnerName -> Not in Registry: [%s]"), *JoinNames(MissingByOwner));
				}
				if (StaleByIndex.Num() > 0)
				{
					//UE_LOG(LogTemp, Warning, TEXT("  Stale Registry UnitIndex -> No longer in world: [%s]"), *JoinInts(StaleByIndex));
				}
				if (StaleByOwner.Num() > 0)
				{
					//UE_LOG(LogTemp, Warning, TEXT("  Stale Registry OwnerName -> No longer in world: [%s]"), *JoinNames(StaleByOwner));
				}
			}
		}
	}
	else
	{
		// Low verbosity success summary
		if (RegLogLevel() >= 2)
		{
			//UE_LOG(LogTemp, Verbose, TEXT("[RTS.Replication] DIAG[%s|%s]: OK LiveUnits=%d Registry=%d"), Context, ModeStr, LiveUnits, RegCount);
		}
	}
}

void AUnitRegistryReplicator::ServerDiagnosticsTick()
{
	if (UWorld* W = GetWorld())
	{
		// First run diagnostics (visible on both server and client when invoked)
		RunDiagnosticsForWorld(*W, Registry, TEXT("RegReplicator"));

		// Server-authoritative cleanup of stale registry entries
		if (W->GetNetMode() != NM_Client)
		{
			// Build sets of currently live actors' keys
			TSet<int32> LiveIndices;
			TSet<FName> LiveOwners;
			for (TActorIterator<AUnitBase> It(W); It; ++It)
			{
				AUnitBase* Unit = *It;
				if (!IsValid(Unit)) continue;
				// Include dead units as live for replication/registry until they despawn
				LiveOwners.Add(Unit->GetFName());
				if (Unit->UnitIndex != INDEX_NONE)
				{
					LiveIndices.Add(Unit->UnitIndex);
				}
			}

			// Build sets of registry keys for quick lookup
			TSet<int32> RegIndices;
			TSet<FName> RegOwners;
			RegIndices.Reserve(Registry.Items.Num());
			RegOwners.Reserve(Registry.Items.Num());
			for (const FUnitRegistryItem& Itm : Registry.Items)
			{
				if (Itm.UnitIndex != INDEX_NONE) { RegIndices.Add(Itm.UnitIndex); }
				if (Itm.OwnerName != NAME_None) { RegOwners.Add(Itm.OwnerName); }
			}

			// Safety net: insert any missing live units into the registry immediately
			int32 Inserted = 0;
			UMassEntitySubsystem* EntitySubsystem = W->GetSubsystem<UMassEntitySubsystem>();
			FMassEntityManager* EM = EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
			for (TActorIterator<AUnitBase> It2(W); It2; ++It2)
			{
				AUnitBase* Unit = *It2;
				if (!IsValid(Unit)) continue;
				if (Unit->UnitState == UnitData::Dead) continue;
				const bool bMissing = (Unit->UnitIndex == INDEX_NONE || !RegIndices.Contains(Unit->UnitIndex)) && !RegOwners.Contains(Unit->GetFName());
				if (bMissing)
				{
					FMassNetworkID NetIDValue;
					bool bHaveNetID = false;
					if (EM)
					{
						if (UMassActorBindingComponent* Bind = Unit->FindComponentByClass<UMassActorBindingComponent>())
						{
							const FMassEntityHandle EHandle = Bind->GetMassEntityHandle();
							if (EHandle.IsSet() && EM->IsEntityValid(EHandle))
							{
								if (FMassNetworkIDFragment* NetFrag = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(EHandle))
								{
									if (NetFrag->NetID.GetValue() != 0)
									{
										NetIDValue = NetFrag->NetID;
										bHaveNetID = true;
									}
									else
									{
										// Assign new NetID if missing
										NetIDValue = FMassNetworkID(GetNextNetID());
										NetFrag->NetID = NetIDValue;
										bHaveNetID = true;
									}
								}
							}
						}
					}
					if (!bHaveNetID)
					{
						// As a last resort, allocate a NetID to keep registry consistent
						NetIDValue = FMassNetworkID(GetNextNetID());
					}
					const int32 NewIdx = Registry.Items.AddDefaulted();
					Registry.Items[NewIdx].OwnerName = Unit->GetFName();
					Registry.Items[NewIdx].UnitIndex = Unit->UnitIndex;
					Registry.Items[NewIdx].NetID = NetIDValue;
					Registry.MarkItemDirty(Registry.Items[NewIdx]);
					Inserted++;
				}
			}

			int32 Removed = 0;
			for (int32 i = Registry.Items.Num() - 1; i >= 0; --i)
			{
				const FUnitRegistryItem& Itm = Registry.Items[i];
				const bool bOwnerGone = (Itm.OwnerName != NAME_None) && !LiveOwners.Contains(Itm.OwnerName);
				const bool bIndexGone = (Itm.UnitIndex != INDEX_NONE) && !LiveIndices.Contains(Itm.UnitIndex);
				if (bOwnerGone || bIndexGone)
				{
					QuarantineNetID(Itm.NetID.GetValue());
					Registry.Items.RemoveAt(i);
					++Removed;
				}
			}
			if (Inserted > 0 || Removed > 0)
			{
				// Mark the array modified to push delta to clients
				Registry.MarkArrayDirty();
				if (Removed > 0)
				{
					//UE_LOG(LogTemp, Warning, TEXT("[RTS.Replication] Server pruned %d stale entries from Unit Registry."), Removed);
				}
				if (Inserted > 0)
				{
					//UE_LOG(LogTemp, Warning, TEXT("[RTS.Replication] Server inserted %d missing live Units into Unit Registry."), Inserted);
				}
			}
		}
	}
}

AUnitRegistryReplicator* AUnitRegistryReplicator::GetOrSpawn(UWorld& World)
{
	// Cache per-world pointer and throttle lookup/spawn attempts to once per second
	static TMap<UWorld*, TWeakObjectPtr<AUnitRegistryReplicator>> GCache;
	static TMap<UWorld*, double> GLastAttempt;
	const double Now = World.GetTimeSeconds();
	if (TWeakObjectPtr<AUnitRegistryReplicator>* Found = GCache.Find(&World))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	double* Last = GLastAttempt.Find(&World);
	if (Last && (Now - *Last) < 1.0)
	{
		// Too soon to try again; return nullptr to avoid per-tick work
		return nullptr;
	}
	GLastAttempt.Add(&World, Now);

	for (TActorIterator<AUnitRegistryReplicator> It(&World); It; ++It)
	{
		GCache.Add(&World, *It);
		return *It;
	}
	if (World.GetNetMode() != NM_Client)
	{
		FActorSpawnParameters Params;
		AUnitRegistryReplicator* Actor = World.SpawnActor<AUnitRegistryReplicator>(AUnitRegistryReplicator::StaticClass(), FTransform::Identity, Params);
		if (Actor)
		{
			Actor->SetReplicates(true);
			Actor->bAlwaysRelevant = true;
			Actor->SetNetUpdateFrequency(10.0f);
			Actor->ForceNetUpdate();
			GCache.Add(&World, Actor);
		}
		return Actor;
	}
	return nullptr;
}

bool AUnitRegistryReplicator::AreAllUnitsRegistered() const
{
	int32 Registered = 0;
	int32 Total = 0;
	GetRegistrationCounts(Registered, Total);
	return (Total == 0) || (Registered >= Total);
}

float AUnitRegistryReplicator::GetRegistrationProgress() const
{
	int32 Registered = 0;
	int32 Total = 0;
	GetRegistrationCounts(Registered, Total);
	if (Total == 0)
	{
		return 1.0f; // No units = fully registered
	}
	return FMath::Clamp(static_cast<float>(Registered) / static_cast<float>(Total), 0.0f, 1.0f);
}

void AUnitRegistryReplicator::GetRegistrationCounts(int32& OutRegistered, int32& OutTotal) const
{
	OutRegistered = 0;
	OutTotal = 0;
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	// Build lookup sets from registry
	TSet<int32> RegIndices;
	TSet<FName> RegOwners;
	RegIndices.Reserve(Registry.Items.Num());
	RegOwners.Reserve(Registry.Items.Num());
	for (const FUnitRegistryItem& It : Registry.Items)
	{
		if (It.UnitIndex != INDEX_NONE)
		{
			RegIndices.Add(It.UnitIndex);
		}
		if (It.OwnerName != NAME_None)
		{
			RegOwners.Add(It.OwnerName);
		}
	}
	
	// Count live units and check if they're registered
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsValid(Unit))
		{
			continue;
		}
		// Skip dead units - they don't need to be registered
		if (Unit->UnitState == UnitData::Dead)
		{
			continue;
		}
		
		OutTotal++;
		
		// Check if this unit is in the registry (by UnitIndex or OwnerName)
		const bool bInRegistry = (Unit->UnitIndex != INDEX_NONE && RegIndices.Contains(Unit->UnitIndex)) 
		                      || RegOwners.Contains(Unit->GetFName());
		if (bInRegistry)
		{
			OutRegistered++;
		}
	}
}