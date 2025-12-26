// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "TimerManager.h"
#include "ExtensionAbility.generated.h"

class USoundBase;
class AWorkArea;
class ABuildingBase;
class AUnitBase;

UCLASS()
class RTSUNITTEMPLATE_API UExtensionAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()
public:
	UExtensionAbility();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AllowAddingWorkers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool UseMousePosition = true;
	
};
