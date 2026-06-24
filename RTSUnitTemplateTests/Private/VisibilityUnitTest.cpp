// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibilityComputeLocalVisibilityTest, "RTSUnitTemplate.Visibility.ComputeLocalVisibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVisibilityComputeLocalVisibilityTest::RunTest(const FString& Parameters)
{
	// Create a dummy world for spawning the actor
	if (!GEngine) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APerformanceUnit* Unit = World->SpawnActor<APerformanceUnit>(APerformanceUnit::StaticClass(), SpawnParams);

	if (!Unit)
	{
		World->DestroyWorld(false);
		return false;
	}

	auto ResetUnit = [](APerformanceUnit* U) {
		U->IsInitialized = true;
		U->bIsInvisible = false;
		U->IsMyTeam = false;
		U->IsVisibleEnemy = false;
		U->IsOnViewport = true;
		U->EnableFog = true;
		U->NetModeOverride = 255;
	};

	// Test Case 1: Not Initialized
	{
		ResetUnit(Unit);
		Unit->IsInitialized = false;
		bool bVis = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 1: IsInitialized=false -> Result=%s"), bVis ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Not initialized should always be invisible"), bVis, false);
	}

	// Test Case 2: Initialized, but invisible enemy (e.g. stealth)
	{
		ResetUnit(Unit);
		Unit->bIsInvisible = true;
		Unit->IsMyTeam = false;
		bool bVis = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 2: bIsInvisible=true, IsMyTeam=false -> Result=%s"), bVis ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Invisible enemy should be invisible"), bVis, false);
	}

	// Test Case 3: Initialized, invisible tag but IS my team (should be visible)
	{
		ResetUnit(Unit);
		Unit->bIsInvisible = true;
		Unit->IsMyTeam = true;
		Unit->EnableFog = false;
		Unit->IsOnViewport = true;
		Unit->NetModeOverride = (uint8)NM_Client;
		bool bVis = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 3: bIsInvisible=true, IsMyTeam=true, EnableFog=false -> Result=%s"), bVis ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Invisible friendly should be visible to self"), bVis, true);
	}

	// Test Case 4: Fog of War - Enemy Hidden
	{
		ResetUnit(Unit);
		Unit->IsMyTeam = false;
		Unit->EnableFog = true;
		Unit->IsVisibleEnemy = false;
		bool bVis = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 4: EnableFog=true, IsVisibleEnemy=false, IsMyTeam=false -> Result=%s"), bVis ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Enemy in Fog should be invisible"), bVis, false);
	}

	// Test Case 5: Local-viewport machines (Standalone / Listen-server host) cull off-viewport
	// units locally; only a Dedicated server (no local view) ignores the viewport.
	{
		ResetUnit(Unit);
		Unit->EnableFog = false; // Fog allows
		Unit->IsOnViewport = false;

		Unit->NetModeOverride = (uint8)NM_DedicatedServer;
		bool bVisDedicated = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 5a: DedicatedServer, IsOnViewport=false -> Result=%s"), bVisDedicated ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Dedicated server has no local viewport -> stays visible"), bVisDedicated, true);

		Unit->NetModeOverride = (uint8)NM_Standalone;
		bool bVisStandalone = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 5b: Standalone, IsOnViewport=false -> Result=%s"), bVisStandalone ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Standalone should cull off-viewport units locally"), bVisStandalone, false);

		Unit->NetModeOverride = (uint8)NM_ListenServer;
		bool bVisListen = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 5c: ListenServer, IsOnViewport=false -> Result=%s"), bVisListen ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Listen-server host should cull off-viewport units locally"), bVisListen, false);
	}

	// Test Case 6: Client Context
	// Should respect viewport for local optimization
	{
		ResetUnit(Unit);
		Unit->NetModeOverride = (uint8)NM_Client;
		Unit->IsOnViewport = false;
		Unit->EnableFog = false;
		
		ENetMode CurrentMode = Unit->GetUnitNetMode();
		bool bVisOff = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 6a: Client Check. NetModeOverride=%d, GetUnitNetMode()=%d, IsOnViewport=%d, bVisOff=%d"), 
			(int32)NM_Client, (int32)CurrentMode, (int32)Unit->IsOnViewport, (int32)bVisOff));
		
		TestEqual(TEXT("Client should respect viewport for local visibility state (Off-screen)"), bVisOff, false);

		Unit->IsOnViewport = true;
		bool bVisOn = Unit->ComputeLocalVisibility();
		AddInfo(FString::Printf(TEXT("Case 6b: Client, IsOnViewport=true -> Result=%s"), bVisOn ? TEXT("true") : TEXT("false")));
		TestEqual(TEXT("Client should respect viewport for local visibility state (On-screen)"), bVisOn, true);
	}

	// Clean up
	Unit->Destroy();
	World->DestroyWorld(false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
