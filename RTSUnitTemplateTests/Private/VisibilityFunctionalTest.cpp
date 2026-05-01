// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "VisibilityFunctionalTest.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

AVisibilityFunctionalTest::AVisibilityFunctionalTest()
{
	Author = TEXT("Junie");
	Description = TEXT("Verifies visibility in a live world.");
	TestLabel = TEXT("RTSUnitTemplate.Visibility.FunctionalTest");
	TestTags = TEXT("RTSUnitTemplate.Visibility");
}

void AVisibilityFunctionalTest::PrepareTest()
{
	Super::PrepareTest();

	// Spawn a unit for testing
	if (UnitClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TestUnit = GetWorld()->SpawnActor<APerformanceUnit>(UnitClass, GetActorLocation() + FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
		
		if (TestUnit)
		{
			AddInfo(TEXT("Test Unit spawned successfully."));
		}
	}
	else
	{
		AddError(TEXT("UnitClass is not set. Please select a unit class in the test actor details."));
	}
}

bool AVisibilityFunctionalTest::IsReady_Implementation()
{
	// Ensure the unit is spawned and initialized
	return TestUnit && TestUnit->IsInitialized;
}

void AVisibilityFunctionalTest::StartTest()
{
	Super::StartTest();

	if (!TestUnit)
	{
		FinishTest(EFunctionalTestResult::Failed, TEXT("Failed to spawn Test Unit."));
		return;
	}

	auto RunStep = [this](bool bMyTeam, bool bVisibleEnemy, bool bOnViewport, bool bExpected, const FString& StepName)
	{
		// Reset state
		TestUnit->IsMyTeam = bMyTeam;
		TestUnit->IsVisibleEnemy = bVisibleEnemy;
		TestUnit->IsOnViewport = bOnViewport;
		TestUnit->IsInitialized = true;
		TestUnit->EnableFog = true;
		TestUnit->bIsInvisible = false;

		bool bActual = TestUnit->ComputeLocalVisibility();
		
		FString LogMsg = FString::Printf(TEXT("%s: MyTeam=%d, VisEnemy=%d, Viewport=%d | Result=%d (Expected=%d)"), 
			*StepName, bMyTeam, bVisibleEnemy, bOnViewport, bActual, bExpected);
		
		AddInfo(LogMsg);
		AssertEqual_Bool(bActual, bExpected, LogMsg);
	};

	// Log NetMode for context
	FString NetModeStr;
	switch (GetWorld()->GetNetMode())
	{
		case NM_Client: NetModeStr = TEXT("Client"); break;
		case NM_DedicatedServer: NetModeStr = TEXT("DedicatedServer"); break;
		case NM_ListenServer: NetModeStr = TEXT("ListenServer"); break;
		case NM_Standalone: NetModeStr = TEXT("Standalone"); break;
		default: NetModeStr = TEXT("Unknown"); break;
	}
	AddInfo(FString::Printf(TEXT("Test Environment NetMode: %s"), *NetModeStr));

	// Scenario 1: Own Team, In Viewport -> Always Visible
	RunStep(true, false, true, true, TEXT("Step 1 (Own Team, In Viewport)"));

	// Scenario 2: Enemy, In Fog -> Invisible
	RunStep(false, false, true, false, TEXT("Step 2 (Enemy in Fog)"));

	// Scenario 3: Enemy, Detected -> Visible
	RunStep(false, true, true, true, TEXT("Step 3 (Detected Enemy)"));

	// Scenario 4: Own Team, Off-screen -> Context dependent
	// On Server/Standalone: Should remain visible (bForceHidden = false)
	// On Client: Should be hidden (Optimization)
	bool bExpectedOffScreen = (GetWorld()->GetNetMode() < NM_Client);
	RunStep(true, false, false, bExpectedOffScreen, TEXT("Step 4 (Off-screen)"));

	FinishTest(EFunctionalTestResult::Succeeded, TEXT("Visibility Functional Test completed."));
}
