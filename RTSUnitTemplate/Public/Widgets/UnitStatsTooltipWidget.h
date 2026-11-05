// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitStatsTooltipWidget.generated.h"

class AUnitBase;
class UTextBlock;
class UWidget;
class UImage;
class UMaterialInterface;
class AUnitPreviewStage;

/**
 * Tooltip / info popup with a unit's live stats: health, shield, damage, attack interval, range, speed, armor,
 * plasma resistance and the talent stats. Blueprint children keep their own layout; the optional TextBlock
 * StatsText receives the stat lines and is collapsed while no unit is set.
 *
 * Used as parent of the unit tooltip (selection buttons call SetUnit) and by ACustomControllerBase for the
 * popup on a plain left click on an enemy (ShowUnitStatsPopup).
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitStatsTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Shows Unit's stats (refreshed every RefreshInterval). bFillHeaderTexts also writes the Blueprint's FText
	 * variables Name / TypeName / Description from the unit - call it before the widget is constructed (added to
	 * the viewport) when the Blueprint copies them into its text blocks on Construct.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetUnit(AUnitBase* InUnit, bool bFillHeaderTexts = false);

	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	AUnitBase* GetUnit() const { return Unit.Get(); }

	/** The stat lines for Unit; empty without a unit or attributes. */
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	static FText BuildUnitStatsText(const AUnitBase* InUnit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.25f;

	/**
	 * 3D window like UUnitWidgetSelector's: a material with a texture parameter UnitPreviewTextureParameter holding
	 * its OWN render target (not the selector's - both can be on screen). None = no 3D window.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview")
	UMaterialInterface* UnitPreviewMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview")
	FName UnitPreviewTextureParameter = TEXT("Preview");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview")
	float UnitPreviewRotationSpeed = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UnitPreview", meta = (ClampMin = "1"))
	float UnitPreviewCaptureRate = 30.f;

	/** Popup mode: the widget removes itself once its unit is dead or gone. */
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bCloseWhenUnitGone = false;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate, meta = (BindWidgetOptional))
	UTextBlock* StatsText = nullptr;

	/** The whole 3D window; collapsed without unit or material. */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|UnitPreview", meta = (BindWidgetOptional))
	UWidget* UnitPreviewBox = nullptr;

	/** Shows UnitPreviewMaterial. */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|UnitPreview", meta = (BindWidgetOptional))
	UImage* UnitPreviewImage = nullptr;

private:
	void Refresh();
	void UpdatePreview();

	UPROPERTY(Transient)
	TObjectPtr<AUnitPreviewStage> PreviewStage;

	TWeakObjectPtr<AUnitBase> Unit;
	float SinceRefresh = 0.f;
	bool bOnScreen = false;
};
