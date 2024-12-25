// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelectorButton.h"
#include "Components/TextBlock.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "UnitWidgetSelector.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitWidgetSelector : public UUserWidget
{
	GENERATED_BODY()

	virtual void NativeConstruct() override;

	//void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Interval in seconds for how often to update the resource display
	const float UpdateInterval = 0.5f;

	FTimerHandle UpdateTimerHandle;

	void StartUpdateTimer();
public:
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateSelectedUnits();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void Update(int AbillityArrayIndex);
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UButton*> AbilityButtons;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class USelectorButton*> SingleSelectButtons;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class USelectorButton*> SelectButtons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UTextBlock*> ButtonLabels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UImage*> UnitIcons;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ChangeAbilityButtonCount(int Count);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GetButtonsFromBP();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetButtonColours(int AIndex);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetButtonIds();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetVisibleButtonCount(int32 Count);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetButtonLabelCount(int32 Count);

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void SetUnitIcons(TArray<AUnitBase*>& Units);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int ShowButtonCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int MaxButtonCount = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int MaxAbilityButtonCount = 10;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AExtendedControllerBase* ControllerBase;
	
};
