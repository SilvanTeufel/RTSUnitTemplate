// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "MoveCommandFunctionalTest.generated.h"

class AUnitBase;
class ACustomControllerBase;
class UMassEntitySubsystem;

/**
 * Funktionaler Test für Bewegungsbefehle und Command Queuing.
 */
UCLASS()
class RTSUNITTEMPLATETESTS_API AMoveCommandFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AMoveCommandFunctionalTest();

	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;

	UPROPERTY(EditAnywhere, Category = "RTS Test")
	TSubclassOf<AUnitBase> UnitClass;

private:
	UPROPERTY()
	AUnitBase* TestUnit;

	UPROPERTY()
	ACustomControllerBase* Controller;

	void TestSimpleMove();
	void TestQueuedMove();
};
