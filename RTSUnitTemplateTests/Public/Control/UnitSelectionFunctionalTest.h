// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "UnitSelectionFunctionalTest.generated.h"

class AUnitBase;
class ACustomControllerBase;
class AHUDBase;

/**
 * Funktionaler Test für die Einheiten-Selektion (Einzel- und Gruppen-Selektion).
 */
UCLASS()
class RTSUNITTEMPLATETESTS_API AUnitSelectionFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AUnitSelectionFunctionalTest();

	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;

	UPROPERTY(EditAnywhere, Category = "RTS Test")
	TSubclassOf<AUnitBase> UnitClass;

private:
	UPROPERTY()
	TArray<AUnitBase*> FriendlyUnits;

	UPROPERTY()
	AUnitBase* EnemyUnit;

	UPROPERTY()
	ACustomControllerBase* Controller;

	UPROPERTY()
	AHUDBase* HUD;

	void TestSingleSelection();
	void TestBoxSelection();
	void TestEnemySelectionFilter();
};
