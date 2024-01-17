// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GAS/AttributeSetBase.h"
#include "Core/UnitData.h"
#include "Core/Talents.h"
#include "GAS/GAS.h"
#include "TalentSaveGame.generated.h"


/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UTalentSaveGame : public USaveGame
{
	GENERATED_BODY()

	

public:
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void PopulateAttributeSaveData(UAttributeSetBase* AttributeSet);
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FLevelData LevelData;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FLevelUpData LevelUpData;
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FAttributeSaveData AttributeSaveData;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	EGASAbilityInputID OffensiveAbilityID;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	EGASAbilityInputID AttackAbilityID;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	EGASAbilityInputID ThrowAbilityID;
};
