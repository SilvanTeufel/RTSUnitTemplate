// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controller/AIController/SpeakingUnitControllerBase.h"
#include "Characters/Unit/HealingUnit.h"
#include "HealingUnitController.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AHealingUnitController : public ASpeakingUnitControllerBase
{
	GENERATED_BODY()
public:
	AHealingUnitController();
	/*
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* Pawn) override;
	*/
	virtual void Tick(float DeltaSeconds) override;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnitControlStateMachine", Keywords = "RTSUnitTemplate UnitControlStateMachine"), Category = RTSUnitTemplate)
		void HealingUnitControlStateMachine(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ChaseHealTarget", Keywords = "RTSUnitTemplate ChaseHealTarget"), Category = RTSUnitTemplate)
		void ChaseHealTarget(AHealingUnit* UnitBase,float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Healing", Keywords = "RTSUnitTemplate Healing"), Category = RTSUnitTemplate)
		void Healing(AHealingUnit* UnitBase,float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "HealPause", Keywords = "RTSUnitTemplate HealPause"), Category = RTSUnitTemplate)
		void HealPause(AHealingUnit* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "HealRun", Keywords = "RTSUnitTemplate HealRun"), Category = RTSUnitTemplate)
		void HealRun(AHealingUnit* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void HealRunUEPathfinding(AHealingUnit* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "HealPatrol", Keywords = "RTSUnitTemplate HealPatrol"), Category = RTSUnitTemplate)
		void HealPatrol(AHealingUnit* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HealPatrolUEPathfinding(AHealingUnit* UnitBase, float DeltaSeconds);
	
	bool bHealActorSpawned = false;
};
