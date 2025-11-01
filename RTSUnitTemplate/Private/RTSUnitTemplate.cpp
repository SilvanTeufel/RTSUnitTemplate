// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "RTSUnitTemplate.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Mass/Replication/ReplicationBootstrap.h"

#define LOCTEXT_NAMESPACE "FRTSUnitTemplateModule"

void FRTSUnitTemplateModule::StartupModule()
{
	// Ensure the Mass replication bubble info class is registered for every world as early as possible
	PreWorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* World, const UWorld::InitializationValues IVS)
	{
		if (World)
		{
			RTSReplicationBootstrap::RegisterForWorld(*World);
		}
	});

	// Also register after world initialization as a safety net (in case Pre init missed a world)
	WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld* World, const UWorld::InitializationValues IVS)
	{
		if (World)
		{
			RTSReplicationBootstrap::RegisterForWorld(*World);
		}
	});

	// Register on map load as well to ensure worlds created via seamless travel or browsing get early registration
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World)
	{
		if (World)
		{
			RTSReplicationBootstrap::RegisterForWorld(*World);
		}
	});
}

void FRTSUnitTemplateModule::ShutdownModule()
{
	if (WorldInitHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
	}
	if (PreWorldInitHandle.IsValid())
	{
		FWorldDelegates::OnPreWorldInitialization.Remove(PreWorldInitHandle);
	}
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	}
	WorldInitHandle = FDelegateHandle();
	PreWorldInitHandle = FDelegateHandle();
	PostLoadMapHandle = FDelegateHandle();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRTSUnitTemplateModule, RTSUnitTemplate);