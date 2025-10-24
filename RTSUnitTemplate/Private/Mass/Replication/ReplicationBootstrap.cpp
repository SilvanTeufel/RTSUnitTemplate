#include "Mass/Replication/ReplicationBootstrap.h"

#include "Engine/World.h"
#include "MassReplicationSubsystem.h"
#include "MassClientBubbleInfoBase.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"

namespace
{
	static TMap<const UWorld*, FMassBubbleInfoClassHandle> GWorldToUnitBubbleHandle;
}

namespace RTSReplicationBootstrap
{
	static bool ShouldRegisterInWorld(const UWorld& World)
	{
		// Only register on server worlds (including listen server) and dedicated server. Never on pure clients.
		const ENetMode Mode = World.GetNetMode();
		return Mode == NM_DedicatedServer || Mode == NM_ListenServer || Mode == NM_Standalone; // Standalone safe no-op
	}

	void RegisterForWorld(UWorld& World)
	{
		if (!ShouldRegisterInWorld(World))
		{
			return;
		}

		if (GWorldToUnitBubbleHandle.Contains(&World))
		{
			return; // already registered for this world
		}

		if (UMassReplicationSubsystem* RepSub = World.GetSubsystem<UMassReplicationSubsystem>())
		{
			const TSubclassOf<AMassClientBubbleInfoBase> BubbleCls = AUnitClientBubbleInfo::StaticClass();

			FMassBubbleInfoClassHandle Handle = RepSub->GetBubbleInfoClassHandle(BubbleCls);
			if (!RepSub->IsBubbleClassHandleValid(Handle))
			{
				Handle = RepSub->RegisterBubbleInfoClass(BubbleCls);
			}

			GWorldToUnitBubbleHandle.Add(&World, Handle);
		}
	}

	FMassBubbleInfoClassHandle GetUnitBubbleHandle(const UWorld* World)
	{
		if (!World)
		{
			return FMassBubbleInfoClassHandle();
		}
		if (const FMassBubbleInfoClassHandle* Found = GWorldToUnitBubbleHandle.Find(World))
		{
			return *Found;
		}
		return FMassBubbleInfoClassHandle();
	}
}
