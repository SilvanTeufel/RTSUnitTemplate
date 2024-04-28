// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

UENUM(BlueprintType)
enum class EGASAbilityInputID : uint8
{
	AbilityOne,
	AbilityTwo,
	AbilityThree,
	AbilityFour,
	AbilityFive,
	AbilitySix,
	Confirm,
	Cancel,
	None,
};