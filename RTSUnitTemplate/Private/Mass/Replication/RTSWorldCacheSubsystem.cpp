#include "Mass/Replication/RTSWorldCacheSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Mass/MassActorBindingComponent.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/Replication/ReplicationBootstrap.h"

URTSWorldCacheSubsystem::URTSWorldCacheSubsystem()
{
}

void URTSWorldCacheSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		// Ensure bubble info class is registered before any clients are added
		RTSReplicationBootstrap::RegisterForWorld(*World);
	}
}

void URTSWorldCacheSubsystem::Deinitialize()
{
	ClearAll();
	Super::Deinitialize();
}

void URTSWorldCacheSubsystem::ClearAll()
{
	CachedRegistry.Reset();
	CachedBubble.Reset();
	BindingByOwnerName.Reset();
	LastBindingRebuildTime = -1000.0;
}

AUnitRegistryReplicator* URTSWorldCacheSubsystem::GetRegistry(bool bAllowSpawnOnServer)
{
	if (CachedRegistry.IsValid())
	{
		return CachedRegistry.Get();
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// Try to find existing
	for (TActorIterator<AUnitRegistryReplicator> It(World); It; ++It)
	{
		CachedRegistry = *It;
		return *It;
	}
	if (bAllowSpawnOnServer && World->GetNetMode() != NM_Client)
	{
		if (AUnitRegistryReplicator* Spawned = AUnitRegistryReplicator::GetOrSpawn(*World))
		{
			CachedRegistry = Spawned;
			return Spawned;
		}
	}
	return nullptr;
}

AUnitClientBubbleInfo* URTSWorldCacheSubsystem::GetBubble(bool bAllowSpawnOnServer)
{
	if (CachedBubble.IsValid())
	{
		return CachedBubble.Get();
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AUnitClientBubbleInfo> It(World); It; ++It)
	{
		CachedBubble = *It;
		return *It;
	}
	if (bAllowSpawnOnServer && World->GetNetMode() != NM_Client)
	{
		FActorSpawnParameters Params;
		AUnitClientBubbleInfo* Bubble = World->SpawnActor<AUnitClientBubbleInfo>(AUnitClientBubbleInfo::StaticClass(), FTransform::Identity, Params);
		if (Bubble)
		{
			Bubble->SetReplicates(true);
			Bubble->SetNetUpdateFrequency(10.0f);
			Bubble->ForceNetUpdate();
			CachedBubble = Bubble;
			return Bubble;
		}
	}
	return nullptr;
}

void URTSWorldCacheSubsystem::RebuildBindingCacheIfNeeded(float IntervalSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if ((Now - LastBindingRebuildTime) < FMath::Max(0.1f, IntervalSeconds))
	{
		return;
	}
	BindingByOwnerName.Reset();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UMassActorBindingComponent* Bind = It->FindComponentByClass<UMassActorBindingComponent>())
		{
			BindingByOwnerName.Add(It->GetFName(), Bind);
		}
	}
	LastBindingRebuildTime = Now;
}

UMassActorBindingComponent* URTSWorldCacheSubsystem::FindBindingByOwnerName(FName OwnerName)
{
	if (TWeakObjectPtr<UMassActorBindingComponent>* Found = BindingByOwnerName.Find(OwnerName))
	{
		return Found->Get();
	}
	return nullptr;
}
