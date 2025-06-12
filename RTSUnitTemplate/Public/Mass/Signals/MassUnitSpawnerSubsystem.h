// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "MassUnitSpawnerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassUnitSpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterUnitForMassCreation(AUnitBase* NewUnit);
	void GetAndClearPendingUnits(TArray<AUnitBase*>& OutPendingUnits);

private:
	UPROPERTY()
	TArray<TObjectPtr<AUnitBase>> PendingUnits;
};
