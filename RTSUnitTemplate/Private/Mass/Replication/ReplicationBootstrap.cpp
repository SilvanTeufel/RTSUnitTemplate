#include "Mass/Replication/ReplicationBootstrap.h"

#include "Engine/World.h"
#include "MassReplicationSubsystem.h"
#include "MassClientBubbleInfoBase.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "TimerManager.h"
#include "Engine/Engine.h" // FWorldDelegates

namespace
{
	static TMap<const UWorld*, FMassBubbleInfoClassHandle> GWorldToUnitBubbleHandle;
	static TMap<const UWorld*, FTimerHandle> GRetryTimers;

	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		if (!World)
		{
			return;
		}
		// Remove any retry timers/bookkeeping associated with this world
		if (FTimerHandle* Handle = GRetryTimers.Find(World))
		{
			// Don't call into the timer manager of a world that's being cleaned up; just forget the handle
			GRetryTimers.Remove(World);
		}
		GWorldToUnitBubbleHandle.Remove(World);
	}

	struct FRTSReplicationBootstrap_Init
	{
		FRTSReplicationBootstrap_Init()
		{
			FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
		}
	};
	static FRTSReplicationBootstrap_Init GRTSReplicationBootstrap_Init;
}

	namespace RTSReplicationBootstrap
	{
		static bool ShouldRegisterInWorld(const UWorld& World)
		{
			// Register in all worlds (server and clients). Clients also need the Bubble class handle to avoid GetBubbleInfoClassHandle errors.
			return true;
		}

		static void RetryRegister(TWeakObjectPtr<UWorld> WeakWorld)
		{
			UWorld* World = WeakWorld.Get();
			if (World == nullptr)
			{
				return;
			}
			// If already registered, stop retrying
			if (GWorldToUnitBubbleHandle.Contains(World))
			{
				if (FTimerHandle* Handle = GRetryTimers.Find(World))
				{
					World->GetTimerManager().ClearTimer(*Handle);
					GRetryTimers.Remove(World);
				}
				return;
			}

			if (UMassReplicationSubsystem* RepSub = World->GetSubsystem<UMassReplicationSubsystem>())
			{
				const TSubclassOf<AMassClientBubbleInfoBase> BubbleCls = AUnitClientBubbleInfo::StaticClass();
				// Register unconditionally to avoid early GetBubbleInfoClassHandle error logs in cook/packaging
				const FMassBubbleInfoClassHandle Handle = RepSub->RegisterBubbleInfoClass(BubbleCls);
				GWorldToUnitBubbleHandle.Add(World, Handle);
				UE_LOG(LogTemp, Log, TEXT("[RTS] Registered UnitClientBubbleInfo for world %s (retry path)"), *World->GetName());

				// Clear retry timer once registered
				if (FTimerHandle* HandlePtr = GRetryTimers.Find(World))
				{
					World->GetTimerManager().ClearTimer(*HandlePtr);
					GRetryTimers.Remove(World);
				}
			}
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
				// Register unconditionally to avoid early GetBubbleInfoClassHandle error logs in cook/packaging
				const FMassBubbleInfoClassHandle Handle = RepSub->RegisterBubbleInfoClass(BubbleCls);
				GWorldToUnitBubbleHandle.Add(&World, Handle);
				UE_LOG(LogTemp, Log, TEXT("[RTS] Registered UnitClientBubbleInfo for world %s (init path)"), *World.GetName());
			}
			else
			{
				// Subsystem not ready yet. Schedule retries on a short timer to avoid race conditions.
				if (!GRetryTimers.Contains(&World))
				{
					FTimerDelegate Del;
					TWeakObjectPtr<UWorld> WeakWorld = &World;
					Del.BindStatic(&RetryRegister, WeakWorld);
					FTimerHandle Handle;
					World.GetTimerManager().SetTimer(Handle, Del, 0.1f, true);
					GRetryTimers.Add(&World, Handle);
					UE_LOG(LogTemp, Log, TEXT("[RTS] Scheduled retry for UnitClientBubbleInfo registration in world %s"), *World.GetName());
				}
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
