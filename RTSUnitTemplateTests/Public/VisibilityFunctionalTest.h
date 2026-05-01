// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "VisibilityFunctionalTest.generated.h"

/**
 * Functional test to verify visibility and Fog of War logic in a live world.
 * This test runs in an actual level, allowing Mass Entity subsystems to be fully initialized.
 */
UCLASS()
class RTSUNITTEMPLATETESTS_API AVisibilityFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AVisibilityFunctionalTest();

	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;

protected:
	UPROPERTY(EditAnywhere, Category = "Test")
	TSubclassOf<class APerformanceUnit> UnitClass;

	UPROPERTY()
	class APerformanceUnit* TestUnit;
};
