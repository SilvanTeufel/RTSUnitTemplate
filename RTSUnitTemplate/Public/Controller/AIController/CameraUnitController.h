// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UnitControllerBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "CameraUnitController.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACameraUnitController : public AUnitControllerBase
{
	GENERATED_BODY()
	
public:

	virtual void BeginPlay() override;
	
	virtual void OnPossess(APawn* PawN) override;
	
	virtual void Tick(float DeltaSeconds) override;


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraUnitRunUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraUnitControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds);
};
