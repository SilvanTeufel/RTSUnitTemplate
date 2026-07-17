// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "AttributeTreeWidget.generated.h"

class ALevelUnit;
class SAttributeTreeWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * UMG wrapper that hosts the radial SAttributeTreeWidget (Slate) and bridges it to the
 * replicated ALevelUnit attribute-tree state. Drive it with SetTargetUnit().
 */
UCLASS()
class RTSUNITTEMPLATE_API UAttributeTreeWidget : public UWidget
{
	GENERATED_BODY()

public:
	UAttributeTreeWidget();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetTargetUnit(ALevelUnit* InUnit);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate")
	ALevelUnit* GetTargetUnit() const { return TargetUnit; }

	// --- Style ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	float RingSpacing = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	float NodeRadius = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style", meta = (ClampMin = "0.0"))
	float TooltipDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor LockedColor = FLinearColor(0.10f, 0.10f, 0.12f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor AvailableColor = FLinearColor(0.20f, 0.65f, 1.00f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor PartialColor = FLinearColor(0.95f, 0.75f, 0.15f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor FullColor = FLinearColor(0.20f, 0.85f, 0.35f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor LinkColor = FLinearColor(0.35f, 0.35f, 0.40f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor RingColor = FLinearColor(1.f, 1.f, 1.f, 0.06f);

	// --- Background ---
	/** Tint of the backdrop drawn behind the tree. Multiplied into BackgroundBrush's own Tint.
	 *  IGNORED when BackgroundBrush holds a MATERIAL - Slate never multiplies a tint into a material
	 *  (it only reaches the graph as VertexColor); use GetBackgroundDynamicMaterial() parameters instead.
	 *  Alpha 0 hides the backdrop VISUAL only - the panel still absorbs every left click (that comes from
	 *  Visibility + the mouse handlers, not from the drawn box). Use Visibility to make it click-through.
	 *  At runtime call SetBackgroundColor(); writing this property from Blueprint does not re-push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Background")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.04f, 0.75f);

	/** Backdrop brush. Leave "Image" empty for the classic solid dim (default - identical to previous
	 *  versions). Assign a Texture, or a Material whose Material Domain is "User Interface".
	 *  A Surface-domain material compiles no Slate shader and draws NOTHING, silently and
	 *  indistinguishably from "no material set" - a Warning is logged to help you spot it.
	 *  Draw As = Image ignores Margin (right for a stretched full-rect material). For a 9-sliced backdrop
	 *  use Box + Margin (UV space 0..1, NOT pixels) + a non-zero Image Size.
	 *  Assign at runtime with SetBackgroundBrushFromMaterial(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Tree|Background")
	FSlateBrush BackgroundBrush;

	/** Optional frame drawn ON TOP of the tree and UNDER the tooltip, around the whole widget rect.
	 *  Disabled by default (Draw As = None) - after picking a Material/Texture you must also set
	 *  Draw As = Border (hollow middle, tiled sides - what a frame wants) or Box (filled).
	 *  9-slice corner size is Image Size * Margin. For a MATERIAL there is no texture to measure, so
	 *  Image Size is the ONLY size source - defaulted to 256x256 with Margin 0.25 (= 64px corners) so it
	 *  works the instant you flip Draw As. For a plain outline, Draw As = RoundedBox needs no material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Tree|Background")
	FSlateBrush BorderBrush;

	/** Inset of the border frame from the widget edges, in local (pre-DPI) Slate units.
	 *  At runtime call SetBorderPadding(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Tree|Background")
	FMargin BorderPadding = FMargin(0.f);

	// --- Background / Border setters. These are the SUPPORTED runtime path: nothing calls
	// SynchronizeProperties outside the editor, so writing the UPROPERTYs directly from Blueprint does
	// not re-push and skips the Material-Domain check. ---
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBackgroundBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBackgroundBrushFromMaterial(UMaterialInterface* Material);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBorderBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBorderBrushFromMaterial(UMaterialInterface* Material);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBackgroundColor(FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetBorderPadding(FMargin InPadding);

	/** Wraps the assigned background material in a MID (Outer = this, which is the keep-alive) so its
	 *  parameters can be driven at runtime. Returns null if BackgroundBrush holds no material. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	UMaterialInstanceDynamic* GetBackgroundDynamicMaterial();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	UMaterialInstanceDynamic* GetBorderDynamicMaterial();

	/** Expand the widget to at least the viewport size so the backdrop fills the screen and the ring
	 *  stays centred. Requires the hosting container to auto-size / fill (not a fixed-size Canvas slot).
	 *  This is also what decides "screen-edge frame" (true + filling container) vs "panel frame"
	 *  (false + a fixed Canvas slot) for BorderBrush. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Background")
	bool bFillViewport = true;

	// --- Text sizes ---
	/** Font size of the per-node "invested/max" count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Text", meta = (ClampMin = "4"))
	int32 CountFontSize = 8;

	/** Font size of the empty-state hint and the Reset button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Text", meta = (ClampMin = "4"))
	int32 NodeFontSize = 9;

	// --- Tooltip ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "4"))
	int32 TooltipTitleFontSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "4"))
	int32 TooltipBodyFontSize = 13;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "50"))
	float TooltipMaxWidth = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "0"))
	float TooltipPadding = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "0"))
	float TooltipIconSize = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "0"))
	float TooltipIconGap = 8.f;

	// --- UWidget interface ---
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Rebuild the node model from the target's AttributeTreeDataTable. */
	void RefreshNodes();

	// Slate live-state / action callbacks
	int32 HandleGetNodePoints(FName NodeId) const;
	int32 HandleGetAvailablePoints() const;
	bool HandleIsUnlocked(FName NodeId) const;
	void HandleInvest(FName NodeId);
	void HandleReset();

	UPROPERTY()
	ALevelUnit* TargetUnit = nullptr;

	TSharedPtr<SAttributeTreeWidget> MyTree;
};
