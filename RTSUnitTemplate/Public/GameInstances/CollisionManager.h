// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CollisionManager.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UCollisionManager : public UGameInstance
{
	GENERATED_BODY()

public:

	// Register a collision event
	void RegisterCollision();

	void ResetCollisionCountOnTime();
	// Check if collisions can still be processed
	bool CanProcessCollision() const;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=RTSUnitTemplate)
	int32 CollisionCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=RTSUnitTemplate)
	int32 MaxCollisionsPerSecond = 40;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=RTSUnitTemplate)
	float TimeWindow =  1.0f;;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=RTSUnitTemplate)
	float LastResetTime = 0.0f;

};