// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputConfig.h"
#include "GameplayTagContainer.h"

#include "EnhancedInputComponentBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UEnhancedInputComponentBase : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:

	template<class UserClass, typename FuncType, typename int32>
	void BindActionByTag(const UInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, int32 CamState);
	DECLARE_DELEGATE_OneParam(SetCamDelegate, int32);
};

template<class UserClass, typename FuncType, typename int32>
void UEnhancedInputComponentBase::BindActionByTag(const UInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, int32 CamState)
{
	if(!InputConfig)
    	return;
    		
	check(InputConfig);
	
	if (const UInputAction* IA = InputConfig->FindInputActionForTag(InputTag))
	{
		BindAction(IA, TriggerEvent, Object, Func, CamState);
	}
	
}