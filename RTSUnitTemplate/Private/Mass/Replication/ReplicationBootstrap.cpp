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
			// Register in all worlds (server and clients). Clients also need the Bubble class handle to avoid GetBubbleInfoClassHandle errors.
			return true;
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
