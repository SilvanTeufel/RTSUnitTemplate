// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "RTSUnitTemplate.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "Controller/Input/GameplayTags.h"
#include "Mass/Replication/ReplicationBootstrap.h"

#define LOCTEXT_NAMESPACE "FRTSUnitTemplateModule"

void FRTSUnitTemplateModule::StartupModule()
{
	// Register the native InputTag.* tags ourselves. They used to be registered only from
	// UAssetManagerBase::StartInitialLoading, which just never runs unless the consuming project sets
	// AssetManagerClassName=/Script/RTSUnitTemplate.AssetManagerBase in its DefaultEngine.ini. Without
	// that line every InputTag is invalid, BindActionByTag binds nothing, and the whole keyboard/mouse
	// setup silently does nothing. Config/Engine.ini ships the setting too; this is the belt to that
	// suspenders, and keeps working if the project supplies its own AssetManager.
	// CallOrRegister runs the delegate immediately when native tags were already flushed.
	NativeTagsHandle = UGameplayTagsManager::CallOrRegister_OnAddNativeTagsDelegate(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&FGameplayTags::RegisterNativeTags));

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
	if (NativeTagsHandle.IsValid())
	{
		UGameplayTagsManager::UnregisterNativeTagDelegate(NativeTagsHandle);
		NativeTagsHandle = FDelegateHandle();
	}
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