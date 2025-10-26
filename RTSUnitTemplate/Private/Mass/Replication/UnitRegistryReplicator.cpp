#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/MassActorBindingComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "TimerManager.h"

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
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().SetTimer(DiagnosticsTimerHandle, this, &AUnitRegistryReplicator::ServerDiagnosticsTick, 5.0f, true, 5.0f);
		}
		if (GRegistryImportantLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("UnitRegistryReplicator: Reset NetID counter to 1 (world=%s)"), *GetWorld()->GetName());
		}
	}
}

void AUnitRegistryReplicator::ResetNetIDCounter()
{
	NextNetID = 1;
}

uint32 AUnitRegistryReplicator::GetNextNetID()
{
	// Return current and increment
	return NextNetID++;
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
		if (Unit->UnitState == UnitData::Dead)
		{
			continue;
		}
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

		UE_LOG(LogTemp, Warning, TEXT("[RTS.Replication] DIAG[%s|%s]: LiveUnits=%d, Registry=%d, Missing(Index)=%d, Missing(Owner)=%d, Stale(Index)=%d, Stale(Owner)=%d"),
			Context, ModeStr, LiveUnits, RegCount, MissingByIndex.Num(), MissingByOwner.Num(), StaleByIndex.Num(), StaleByOwner.Num());

		if (MissingByIndex.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Missing UnitIndex -> Not in Registry: [%s]"), *JoinInts(MissingByIndex));
		}
		if (MissingByOwner.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Missing OwnerName -> Not in Registry: [%s]"), *JoinNames(MissingByOwner));
		}
		if (StaleByIndex.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Stale Registry UnitIndex -> No longer in world: [%s]"), *JoinInts(StaleByIndex));
		}
		if (StaleByOwner.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Stale Registry OwnerName -> No longer in world: [%s]"), *JoinNames(StaleByOwner));
		}
	}
	else
	{
		// Low verbosity success summary
		UE_LOG(LogTemp, Verbose, TEXT("[RTS.Replication] DIAG[%s|%s]: OK LiveUnits=%d Registry=%d"), Context, ModeStr, LiveUnits, RegCount);
	}
}

void AUnitRegistryReplicator::ServerDiagnosticsTick()
{
	if (UWorld* W = GetWorld())
	{
		RunDiagnosticsForWorld(*W, Registry, TEXT("RegReplicator"));
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