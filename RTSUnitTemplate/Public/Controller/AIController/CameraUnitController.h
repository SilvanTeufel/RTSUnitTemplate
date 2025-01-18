// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "CameraUnitController.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACameraUnitController : public AAIController
{
	GENERATED_BODY()
	
public:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	AUnitBase* MyUnitBase;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	AExtendedControllerBase* ControllerBase;
	
	virtual void OnPossess(APawn* PawN) override;
	
	virtual void Tick(float DeltaSeconds) override;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Casting(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RotateToAttackUnit(AUnitBase* UnitBase, AUnitBase* UnitToChase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UnitControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds);
};
