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

	auto RunStep = [this](bool bMyTeam, bool bVisibleEnemy, bool bOnViewport, bool bExpectedActor, bool bExpectedInherent, const FString& StepName)
	{
		// Reset state
		TestUnit->IsMyTeam = bMyTeam;
		TestUnit->IsVisibleEnemy = bVisibleEnemy;
		TestUnit->IsOnViewport = bOnViewport;
		TestUnit->IsInitialized = true;
		TestUnit->EnableFog = true;
		TestUnit->bIsInvisible = false;

		// 1) Actor check (Local visibility including Viewport on Client)
		bool bActualActor = TestUnit->ComputeLocalVisibility();
		
		FString LogMsgActor = FString::Printf(TEXT("%s (Actor): MyTeam=%d, VisEnemy=%d, Viewport=%d | Result=%d (Expected=%d)"), 
			*StepName, bMyTeam, bVisibleEnemy, bOnViewport, bActualActor, bExpectedActor);
		
		AddInfo(LogMsgActor);
		AssertEqual_Bool(bActualActor, bExpectedActor, LogMsgActor);

		// 2) Inherent check (Visibility without Viewport, used for replication/ISMs)
		bool bActualInherent = TestUnit->ComputeInherentVisibility();
		
		FString LogMsgInherent = FString::Printf(TEXT("%s (Inherent): MyTeam=%d, VisEnemy=%d, Viewport=%d | Result=%d (Expected=%d)"), 
			*StepName, bMyTeam, bVisibleEnemy, bOnViewport, bActualInherent, bExpectedInherent);
		
		AddInfo(LogMsgInherent);
		AssertEqual_Bool(bActualInherent, bExpectedInherent, LogMsgInherent);
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
	RunStep(true, false, true, true, true, TEXT("Step 1 (Own Team, In Viewport)"));

	// Scenario 2: Enemy, In Fog -> Invisible
	RunStep(false, false, true, false, false, TEXT("Step 2 (Enemy in Fog)"));

	// Scenario 3: Enemy, Detected -> Visible
	RunStep(false, true, true, true, true, TEXT("Step 3 (Detected Enemy)"));

	// Scenario 4: Own Team, Off-screen
	// Actor (SkelMesh) on Client should be hidden (optimization), 
	// but Inherent (ISMs) should stay visible (to avoid server-viewport-poisoning).
	bool bExpectedActorOffScreen = (GetWorld()->GetNetMode() < NM_Client);
	bool bExpectedInherentOffScreen = true; // Inherent visibility should ignore viewport
	
	RunStep(true, false, false, bExpectedActorOffScreen, bExpectedInherentOffScreen, TEXT("Step 4 (Off-screen)"));

	FinishTest(EFunctionalTestResult::Succeeded, TEXT("Visibility Functional Test completed."));
}
