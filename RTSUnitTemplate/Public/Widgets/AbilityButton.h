// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "AbilityButton.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FButtonIdDelegate, int, New_Text_Id);

UCLASS()
class RTSUNITTEMPLATE_API UAbilityButton : public UButton
{
	GENERATED_BODY()

	
public:
	UAbilityButton();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int Id = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FButtonIdDelegate CallbackIdDelegate;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnClick();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbility(int AbilityIndex);
	
};
