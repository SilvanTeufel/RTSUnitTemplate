#include "Mass/Replication/RTSWorldCacheSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Mass/MassActorBindingComponent.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/Replication/ReplicationBootstrap.h"
#include "Characters/Unit/UnitBase.h"
#include "HAL/IConsoleManager.h"

URTSWorldCacheSubsystem::URTSWorldCacheSubsystem()
{
}

void URTSWorldCacheSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UWorld* World = GetWorld();
	if (World)
	{
		// Ensure bubble info class is registered for all worlds (server and clients)
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
	BindingByUnitIndex.Reset();
	LastBindingRebuildTime = -1000.0;
	PendingSkipMoveNetIDs.Reset();
	PendingSkipMoveOwnerNames.Reset();
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
			float Hz = 10.0f;
			if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("net.RTS.Bubble.NetUpdateHz")))
			{
				Hz = FMath::Max(0.1f, Var->GetFloat());
			}
			Bubble->SetNetUpdateFrequency(Hz);
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
	BindingByUnitIndex.Reset();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UMassActorBindingComponent* Bind = It->FindComponentByClass<UMassActorBindingComponent>())
		{
			BindingByOwnerName.Add(It->GetFName(), Bind);
			if (const AUnitBase* Unit = Cast<AUnitBase>(*It))
			{
				if (Unit->UnitIndex != INDEX_NONE)
				{
					BindingByUnitIndex.Add(Unit->UnitIndex, Bind);
				}
			}
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

UMassActorBindingComponent* URTSWorldCacheSubsystem::FindBindingByUnitIndex(int32 UnitIndex)
{
	if (TWeakObjectPtr<UMassActorBindingComponent>* Found = BindingByUnitIndex.Find(UnitIndex))
	{
		return Found->Get();
	}
	return nullptr;
}


void URTSWorldCacheSubsystem::MarkSkipMoveForNetID(uint32 NetID)
{
	if (NetID != 0)
	{
		PendingSkipMoveNetIDs.Add(NetID);
	}
}

bool URTSWorldCacheSubsystem::HasSkipMoveForNetID(uint32 NetID) const
{
	return NetID != 0 && PendingSkipMoveNetIDs.Contains(NetID);
}

bool URTSWorldCacheSubsystem::ConsumeSkipMoveForNetID(uint32 NetID)
{
	if (NetID == 0)
	{
		return false;
	}
	return PendingSkipMoveNetIDs.Remove(NetID) > 0;
}

void URTSWorldCacheSubsystem::MarkSkipMoveForOwnerName(FName OwnerName)
{
	if (OwnerName != NAME_None)
	{
		PendingSkipMoveOwnerNames.Add(OwnerName);
	}
}

bool URTSWorldCacheSubsystem::HasSkipMoveForOwnerName(FName OwnerName) const
{
	return OwnerName != NAME_None && PendingSkipMoveOwnerNames.Contains(OwnerName);
}

bool URTSWorldCacheSubsystem::ConsumeSkipMoveForOwnerName(FName OwnerName)
{
	if (OwnerName == NAME_None)
	{
		return false;
	}
	return PendingSkipMoveOwnerNames.Remove(OwnerName) > 0;
}
