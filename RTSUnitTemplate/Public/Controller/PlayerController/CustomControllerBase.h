// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
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
	
};
