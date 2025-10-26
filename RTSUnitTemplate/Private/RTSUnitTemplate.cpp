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
	WorldInitHandle = FDelegateHandle();
	PreWorldInitHandle = FDelegateHandle();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRTSUnitTemplateModule, RTSUnitTemplate);