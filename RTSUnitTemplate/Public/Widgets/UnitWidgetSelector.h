// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelectorButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
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
	const float UpdateInterval = 0.25f;

	FTimerHandle UpdateTimerHandle;

	void StartUpdateTimer();
public:

	// Replaces rarity keywords in text with specified values
	UFUNCTION(BlueprintCallable, Category = "Text Processing")
	FText ReplaceRarityKeywords(
		FText OriginalText,
		FText NewPrimary,  // Default values act as fallback
		FText NewSecondary,
		FText NewTertiary,
		FText NewRare,
		FText NewEpic,
		FText NewLegendary
	);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateSelectedUnits();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateAbilityCooldowns();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateCurrentAbility();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateQueuedAbilityIcons();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnAbilityQueueButtonClicked(int32 ButtonIndex);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnCurrentAbilityButtonClicked();
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void Update(int AbillityArrayIndex);
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Name;

	UPROPERTY(meta = (BindWidget))
	class UImage* CurrentAbilityIcon;

	UPROPERTY(meta = (BindWidget))
	class UButton* CurrentAbilityButton;
		
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* CurrentAbilityTimerBar;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor CurrentAbilityTimerBarColor = FLinearColor::Black;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UButton*> AbilityQueButtons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UImage*> AbilityQueIcons;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UButton*> AbilityButtons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<UTextBlock*> AbilityCooldownTexts;
	
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
	int MaxQueButtonCount = 5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AExtendedControllerBase* ControllerBase;
	
};
