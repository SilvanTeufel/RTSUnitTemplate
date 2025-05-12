// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"


#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassCommandBuffer.h"
#include "MassNavigationFragments.h"
#include "MassMovementFragments.h"
#include "Engine/World.h"        // Include for UWorld, GEngine
#include "Engine/Engine.h"       // Include for GEngine


#include "CustomControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACustomControllerBase : public AExtendedControllerBase
{
	GENERATED_BODY()
public:
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetFogManager(const TArray<AActor*>& AllUnits);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetFogManagerUnit(APerformanceUnit* Unit);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_ShowWidgetsWhenLocallyControlled();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetCamLocation(FVector NewLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_HideEnemyWaypoints();


	
	UFUNCTION(Client, Reliable)
	void AgentInit();

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void CorrectSetUnitMoveTarget(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed = 300.0f, float AcceptanceRadius = 50.0f);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickPressedMass();


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RunUnitsAndSetWaypointsMass(FHitResult Hit);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickPressedMass();

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void LeftClickAttackMass(AUnitBase* Unit, FVector Location);

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void LeftClickAMoveUEPFMass(AUnitBase* Unit, FVector Location);


	UFUNCTION(Server, Reliable,  Category = RTSUnitTemplate)
	void Server_ReportUnitVisibility(APerformanceUnit* Unit, bool bVisible);
};
