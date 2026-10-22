// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "TutorialWidget.generated.h"

class SBox;
class STextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTutorialWidgetClosed);

/**
 * Tutorial hint panel (Slate): a framed text box at the top centre of the screen, shown by ATutorialActor one
 * step after the other. Frame, overlay and fonts are picked in the Blueprint child (e.g. WBP_Tutorial_AH);
 * the widget tree of that Blueprint stays empty. Never takes the mouse - clicks go through to the game - except on
 * the close button (X), which ends the tutorial.
 */
UCLASS()
class RTSUNITTEMPLATE_API UTutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTutorialWidget(const FObjectInitializer& ObjectInitializer);

	/** Fades the current text out and Text in. Index / Count (0-based / total) feed the step counter. */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ShowStep(const FText& Text, int32 Index, int32 Count);

	/** Fades the panel out (the tutorial is done or skipped). */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void HideTutorial();

	/** True while the panel is (fading) out of view. */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsHidden() const { return !bWantVisible && Opacity <= 0.f; }

	// --- Frame ------------------------------------------------------------------------------------

	/** Background / frame of the panel - a material (e.g. MI_SciFi_Panel) or texture, drawn as set in the brush. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Frame")
	FSlateBrush FrameBrush;

	/** Drawn over the whole panel (e.g. the flicker MI_StoryFlicker_AH). Empty = nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Frame")
	FSlateBrush OverlayBrush;

	/** Line under the header (e.g. Tooltip_Header_border). Empty = nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Frame")
	FSlateBrush HeaderBrush;

	/** Space between the frame and the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Frame")
	FMargin ContentPadding = FMargin(28.f, 14.f, 28.f, 18.f);

	/** Width of the panel; the text wraps inside it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Frame", meta = (ClampMin = "100"))
	float PanelWidth = 640.f;

	/** Distance of the panel from the top of the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Frame", meta = (ClampMin = "0"))
	float TopOffset = 110.f;

	// --- Text -------------------------------------------------------------------------------------

	/** Font of the hint text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Text")
	FSlateFontInfo TextFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Text")
	FLinearColor TextColor = FLinearColor(0.82f, 0.89f, 0.96f, 1.f);

	/** Small title above the text, e.g. "TUTORIAL". Empty = no header. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Text")
	FText HeaderText;

	/** Font of the header and of the step counter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Text")
	FSlateFontInfo HeaderFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Text")
	FLinearColor HeaderColor = FLinearColor(0.209f, 0.644f, 0.694f, 1.f);

	/** Shows "2 / 4" beside the header. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Text")
	bool bShowStepCounter = true;

	// --- Close button -----------------------------------------------------------------------------

	/** An X in the header that closes the tutorial for good (ATutorialActor stops it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Close")
	bool bShowCloseButton = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Close")
	FText CloseButtonText;

	/** Look of the X. By default no background, a faint plate while hovered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Close")
	FButtonStyle CloseButtonStyle;

	/** The player clicked the X. */
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FTutorialWidgetClosed OnCloseClicked;

	// --- Animation --------------------------------------------------------------------------------

	/** Seconds to fade a text out / in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Animation", meta = (ClampMin = "0.01"))
	float FadeTime = 0.3f;

	/** How far the panel slides down while fading in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Animation", meta = (ClampMin = "0"))
	float SlideDistance = 12.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplyOpacity();
	FText MakeCounterText() const;
	FReply HandleCloseClicked();

	TSharedPtr<SBox> PanelBox;
	TSharedPtr<STextBlock> TextBlock;
	TSharedPtr<STextBlock> CounterBlock;

	FText CurrentText;
	FText PendingText;
	int32 StepIndex = 0;
	int32 StepCount = 0;
	int32 PendingIndex = 0;
	bool bHasPending = false;
	bool bWantVisible = false;
	float Opacity = 0.f;
};
