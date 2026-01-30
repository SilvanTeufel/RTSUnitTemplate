// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ControlWidget.generated.h"

class UCheckBox;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* SwapScrollCheckBox;

	UFUNCTION()
	void OnSwapScrollChanged(bool bIsChecked);
};
