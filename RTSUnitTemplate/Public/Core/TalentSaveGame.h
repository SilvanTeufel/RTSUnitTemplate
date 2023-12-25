// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GAS/AttributeSetBase.h"
#include "Core/UnitData.h"
#include "Core/Talents.h"
#include "TalentSaveGame.generated.h"


/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UTalentSaveGame : public USaveGame
{
	GENERATED_BODY()

	

public:
	UFUNCTION(BlueprintCallable)
	void PopulateAttributeSaveData(UAttributeSetBase* AttributeSet);
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FLevelData LevelData;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FLevelUpData LevelUpData;
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FAttributeSaveData AttributeSaveData;
};
