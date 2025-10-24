#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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
		UE_LOG(LogTemp, Log, TEXT("UnitRegistryReplicator: Reset NetID counter to 1 (world=%s)"), *GetWorld()->GetName());
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
	// Log the authoritative mapping when it updates on clients
	#if !UE_SERVER
	{
		const int32 Num = Registry.Items.Num();
		const int32 MaxLog = FMath::Min(25, Num);
		FString MapStr;
		for (int32 i = 0; i < MaxLog; ++i)
		{
			const FUnitRegistryItem& It = Registry.Items[i];
			if (i > 0) { MapStr += TEXT(" | "); }
			MapStr += FString::Printf(TEXT("%s->%u"), *It.OwnerName.ToString(), It.NetID.GetValue());
		}
		UE_LOG(LogTemp, Log, TEXT("ClientRegistry(OnRep): %d entries. %s%s"), Num, *MapStr, (Num > MaxLog ? TEXT(" ...") : TEXT("")));
	}
	#endif
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