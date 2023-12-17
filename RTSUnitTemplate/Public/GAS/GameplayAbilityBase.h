// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilityBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGameplayAbilityBase();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
	EGASAbilityInputID AbilityInputID = EGASAbilityInputID::None;

	
};
