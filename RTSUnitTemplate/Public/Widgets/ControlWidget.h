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

	UPROPERTY(meta = (BindWidget))
	UCheckBox* SwapAttackMoveCheckBox;

	/**
	 * Dreht die Hochachse beim Verstellen des Kamerawinkels.
	 *
	 * BindWidgetOptional und nicht BindWidget: die beiden anderen Kaestchen gibt es in jedem
	 * Widget-Blueprint seit jeher, dieses hier nicht. Mit dem strengen BindWidget wuerde jeder
	 * bestehende ControlWidget-Blueprint - auch in RTSUnitExample und Lux - die Kompilierung
	 * verweigern, bis jemand das Kaestchen von Hand nachtraegt.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* InvertCameraAngleCheckBox;

	UFUNCTION()
	void OnSwapScrollChanged(bool bIsChecked);

	UFUNCTION()
	void OnSwapAttackMoveChanged(bool bIsChecked);

	UFUNCTION()
	void OnInvertCameraAngleChanged(bool bIsChecked);
};
