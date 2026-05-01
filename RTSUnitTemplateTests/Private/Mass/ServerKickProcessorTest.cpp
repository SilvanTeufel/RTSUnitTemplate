// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Mass/Replication/ServerReplicationKickProcessor.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "MassEntityTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FServerKickProcessorAuthorityTest, "RTSUnitTemplate.Mass.ServerKickProcessorAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Dieser Test stellt sicher, dass der ServerReplicationKickProcessor 
 * nur auf dem Server oder im Standalone-Modus ausgefuehrt wird.
 */
bool FServerKickProcessorAuthorityTest::RunTest(const FString& Parameters)
{
	// 1. Instanziierung des Prozessors
	UServerReplicationKickProcessor* Processor = NewObject<UServerReplicationKickProcessor>();
	if (!Processor)
	{
		AddError(TEXT("Failed to create UServerReplicationKickProcessor"));
		return false;
	}

	// 2. Pruefung der Execution Flags
	// Diese Flags steuern, ob Mass den Prozessor ueberhaupt in den Pipeline-Graph aufnimmt
	const int32 Flags = (int32)Processor->GetExecutionFlags();
	
	const bool bHasClientFlag = (Flags & (int32)EProcessorExecutionFlags::Client) != 0;
	const bool bHasServerFlag = (Flags & (int32)EProcessorExecutionFlags::Server) != 0;
	const bool bHasStandaloneFlag = (Flags & (int32)EProcessorExecutionFlags::Standalone) != 0;

	TestFalse(TEXT("ServerReplicationKickProcessor darf NICHT das Client-Flag haben"), bHasClientFlag);
	TestTrue(TEXT("ServerReplicationKickProcessor muss das Server-Flag haben"), bHasServerFlag);
	TestTrue(TEXT("ServerReplicationKickProcessor muss das Standalone-Flag haben"), bHasStandaloneFlag);

	// 3. Verifizierung der Processing Phase
	// Er sollte in PrePhysics laufen
	TestEqual(TEXT("Processor sollte in PrePhysics laufen"), Processor->GetProcessingPhase(), EMassProcessingPhase::PrePhysics);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
