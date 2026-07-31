// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Controller/PlayerController/ControllerBase.h" // EGridShape
#include "FormationSelectorWidget.generated.h"

class UHorizontalBox;
class UImage;
class UTexture2D;

/**
 * UButton::OnClicked carries no payload, so a bank of identical buttons cannot tell the owner
 * which one was pressed. Same problem USelectorButton and UTaggedUnitButton solve for their own
 * cases; this is the formation-shaped version of that idiom.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFormationShapeChosen, EGridShape, Shape);

UCLASS()
class RTSUNITTEMPLATE_API UFormationShapeButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGridShape Shape = EGridShape::Square;

	UPROPERTY(BlueprintAssignable, Category = RTSUnitTemplate)
	FOnFormationShapeChosen OnShapeChosen;

	/** Bound to this button's own OnClicked; re-broadcasts with the shape attached. */
	UFUNCTION()
	void HandleClicked();
};

USTRUCT(BlueprintType)
struct FFormationSelectorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGridShape Shape = EGridShape::Square;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TObjectPtr<UTexture2D> Icon = nullptr;
};

/**
 * Formation picker: one icon button per shape, so the player can jump straight to a formation
 * instead of cycling with C. Both routes go through AControllerBase::SetGridFormationShape, so the
 * hotkey and this widget can never disagree - and the highlight follows the hotkey too.
 *
 * Wiring follows the plugin's standard widget pattern: place this inside BP_MainHUD / BP_MainHUD_2
 * and assign it to AExtendedCameraBase::FormationSelectorWidget in EventPreConstruct.
 *
 * The buttons are built in C++ from the Shapes array rather than hand-authored in UMG, so adding
 * an EGridShape later needs no UMG work. The WBP only has to provide a UHorizontalBox named
 * ButtonBox (and even that is optional - one is created if it is missing).
 */
UCLASS()
class RTSUNITTEMPLATE_API UFormationSelectorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFormationSelectorWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Container the buttons are added to. Optional: created on demand when the WBP has none. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = RTSUnitTemplate)
	UHorizontalBox* ButtonBox;

	/** One button per entry, in order. Defaults to all six shapes with the generated icons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector")
	TArray<FFormationSelectorEntry> Shapes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector")
	FLinearColor SelectedTint = FLinearColor(0.235f, 0.86f, 0.353f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector")
	FLinearColor UnselectedTint = FLinearColor(1.f, 1.f, 1.f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector", meta = (ClampMin = "8.0"))
	FVector2D ButtonSize = FVector2D(44.f, 44.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector", meta = (ClampMin = "0.0"))
	float ButtonPadding = 4.f;

	/**
	 * The buttons are built in C++, so they cannot be restyled in the UMG designer. Without this
	 * they would use the engine default (a light grey plate) and the white icons would nearly
	 * vanish on it. Set bOverrideButtonStyle false to fall back to the engine/WBP default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector")
	bool bOverrideButtonStyle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector", meta = (EditCondition = "bOverrideButtonStyle"))
	FLinearColor ButtonNormalColor = FLinearColor(0.015f, 0.017f, 0.025f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector", meta = (EditCondition = "bOverrideButtonStyle"))
	FLinearColor ButtonHoveredColor = FLinearColor(0.10f, 0.12f, 0.15f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Selector", meta = (EditCondition = "bOverrideButtonStyle"))
	FLinearColor ButtonPressedColor = FLinearColor(0.22f, 0.26f, 0.32f, 0.95f);

	/** Destroys and re-creates the button row from Shapes. Safe to call at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Formation Selector")
	void RebuildButtons();

	/** Re-tints the buttons so the active shape is highlighted. */
	UFUNCTION(BlueprintCallable, Category = "Formation Selector")
	void RefreshSelection();

protected:
	UFUNCTION()
	void HandleShapeChosen(EGridShape Shape);

	/** Bound to AControllerBase::OnGridFormationShapeChanged so the C hotkey moves the highlight. */
	UFUNCTION()
	void HandleShapeChangedExternally(EGridShape NewShape);

	/** The owning player controller as AControllerBase, or null. */
	AControllerBase* GetRTSController() const;

	/**
	 * Subscribes to the controller's shape-changed delegate. The controller may not exist yet at
	 * NativeConstruct time, so this retries on a timer until it succeeds - and then stops. No
	 * per-frame polling: widget ticking is off by default for a widget with no BP Tick event.
	 */
	void TryBindToController();

	UPROPERTY(Transient)
	TArray<UFormationShapeButton*> ShapeButtons;

	UPROPERTY(Transient)
	TArray<UImage*> ShapeImages;

	UPROPERTY(Transient)
	TWeakObjectPtr<AControllerBase> BoundController;

	FTimerHandle BindRetryTimerHandle;
};
