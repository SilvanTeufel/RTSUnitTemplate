// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUniteTemplate")
	UTexture2D* AbilityIcon;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTSUniteTemplate")
	EGASAbilityInputID AbilityInputID = EGASAbilityInputID::None;

	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUniteTemplate")
	void OnAbilityCastComplete();

};
