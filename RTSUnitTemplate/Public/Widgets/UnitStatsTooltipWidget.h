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
class AActor;
class UGameplayAbilityBase;
class UWrapBox;

/**
 * The values a unit card shows. Taken from a live unit (UUnitStatsTooltipWidget::CaptureUnitStats) or from a
 * recording - the replay shows cards for its proxies, which have no attributes of their own.
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FUnitStatsValues
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Health = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxHealth = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Shield = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxShield = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AttackDamage = 0.f;
	/** AttackDuration + PauseDuration; 0 = no attack speed line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AttackInterval = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Range = 0.f;
	/** Below 0: the unit cannot move (no speed line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float RunSpeed = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Armor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MagicResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Stamina = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AttackPower = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Willpower = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Haste = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float HealthRegeneration = 0.f;

	/** Automatic abilities the unit had (recorded by the replay; a live unit's card reads AutoAbilityStatus). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<TSubclassOf<UGameplayAbilityBase>> AutoAbilities;
};

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

	/** Abilities (DefaultAbilities) and automatic abilities (AutoAbilityStatus, with their cooldown) of a unit. */
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	static FText BuildAbilitiesText(const AUnitBase* InUnit);

	/** The stat lines for Unit; empty without a unit or attributes. */
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	static FText BuildUnitStatsText(const AUnitBase* InUnit);

	/** Reads the card values of a live unit. False without a unit or attributes. */
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	static bool CaptureUnitStats(const AUnitBase* InUnit, FUnitStatsValues& OutValues);

	/** The stat lines for the given values - the same lines BuildUnitStatsText writes for a live unit. */
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	static FText BuildStatsText(const FUnitStatsValues& Values);

	/**
	 * Card for something that is no live unit (a replay proxy): Name / TypeName / Description from UnitClass's
	 * defaults (bFillHeaderTexts, before construction like SetUnit), the stat lines from Values, the 3D window
	 * built from PreviewSource's meshes. With bCloseWhenUnitGone the card closes once PreviewSource is gone.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetStatsSnapshot(TSubclassOf<AUnitBase> UnitClass, const FUnitStatsValues& Values, AActor* PreviewSource, bool bFillHeaderTexts = false);

	/** New values for the card set up with SetStatsSnapshot (the replay calls this while it plays). */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateStatsSnapshot(const FUnitStatsValues& Values);

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
	FUnitStatsValues Snapshot;
	TWeakObjectPtr<AActor> SnapshotPreview;
	TSubclassOf<AUnitBase> SnapshotClass;
	bool bHasSnapshot = false;

	/**
	 * Icons of the abilities (plain frame) and automatic abilities (gold frame), name + description as hover
	 * tooltip. Built in code into StatsText's parent, right below StatsText. False if there is no place for it
	 * (then the abilities stay text lines in StatsText).
	 */
	bool RefreshAbilityIcons(const AUnitBase* Live);
	UPROPERTY(Transient)
	TObjectPtr<UWrapBox> AbilityIconBox;
	FString AbilityIconSignature;
	float SinceRefresh = 0.f;
	bool bOnScreen = false;
};
