// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelectorButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Controller/PlayerController/CustomControllerBase.h"

#include "UnitWidgetSelector.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitWidgetSelector : public UUserWidget
{
	GENERATED_BODY()

	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Interval in seconds for how often to update the resource display
	const float UpdateInterval = 0.25f;

	FTimerHandle UpdateTimerHandle;

	void StartUpdateTimer();

	/** Shows Unit in the 3D window, or hides the window (nullptr / no material). */
	void UpdateUnitPreview(AUnitBase* Unit);

	UPROPERTY(Transient)
	TObjectPtr<class AUnitPreviewStage> UnitPreviewStage;

	/** What the buttons showed at the last update - a change means the open tooltip shows something else. */
	int32 LastTooltipAbilityArrayIndex = INDEX_NONE;
	int32 LastTooltipUnitWidgetIndex = INDEX_NONE;
	TWeakObjectPtr<AUnitBase> LastTooltipUnit;
public:
	/**
	 * Closes the open tooltip so Slate builds it again for the widget under the cursor. The tooltips of the
	 * ability and select buttons are Blueprint bindings that Slate evaluates only when a tooltip opens - scrolling
	 * through the abilities or units kept showing the old one until the mouse left the button.
	 */
	static void RefreshOpenTooltip();

	void InitWidget(ACustomControllerBase* InController);
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
	void SetWidgetCooldown(int32 AbilityIndex, float RemainingTime);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateAbilityCooldowns();

	// Update enabled/disabled state of ability buttons based on ability flags and team key
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateAbilityButtonsState();

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (BindWidget))
	class UTextBlock* Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (BindWidget))
	class UImage* CurrentAbilityIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (BindWidget))
	class UButton* CurrentAbilityButton;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (BindWidget))
	class UProgressBar* CurrentAbilityTimerBar;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor CurrentAbilityTimerBarColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UButton*> AbilityQueButtons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UImage*> AbilityQueIcons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UUserWidget*> AbilityQueWidgets;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UButton*> AbilityButtons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UUserWidget*> AbilityButtonWidgets;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<UTextBlock*> AbilityCooldownTexts;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class USelectorButton*> SingleSelectButtons;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class USelectorButton*> SelectButtons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UUserWidget*> SelectButtonWidgets;
	
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
	int MaxQueButtonCount = 6;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ACustomControllerBase* ControllerBase;

	/**
	 * 3D window: a turning view of the unit shown in the selector. The material must have a texture
	 * parameter UnitPreviewTextureParameter holding a render target (e.g. M_UnitPreview_AH with
	 * RT_UnitPreview_AH); AUnitPreviewStage renders the unit into it. None = the window stays hidden
	 * and nothing is rendered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview")
	class UMaterialInterface* UnitPreviewMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview")
	FName UnitPreviewTextureParameter = TEXT("Preview");

	/** Degrees per second of the turntable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview")
	float UnitPreviewRotationSpeed = 30.f;

	/** Renders of the 3D window per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview", meta = (ClampMin = "1"))
	float UnitPreviewCaptureRate = 30.f;

	/** The whole window (frame included); collapsed without material or unit. */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|UnitPreview", meta = (BindWidgetOptional))
	class UWidget* UnitPreviewBox = nullptr;

	/** Shows UnitPreviewMaterial. */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|UnitPreview", meta = (BindWidgetOptional))
	class UImage* UnitPreviewImage = nullptr;
	
};
