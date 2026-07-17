// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"

class SBox;
class SImage;
class STextBlock;

/** Static (geometry + presentation) data for a single attribute-tree node. */
struct FAttributeTreeSlateNode
{
	FName Id = NAME_None;
	FName PrevId = NAME_None;
	int32 Ring = 0;
	float Angle = 0.f;
	int32 MaxPoints = 1;
	FText DisplayName;
	FText ToolTipTitle;
	FText ToolTipText;
	FLinearColor BaseColor = FLinearColor(0.15f, 0.55f, 0.95f, 1.f);
	// FSlateBrush by value inside a non-UObject SWidget -> its ResourceObject is NOT GC-rooted by us.
	// Safe only because the source DataTable row keeps the icon texture referenced while the tree is
	// shown. Not a precedent for a material brush: a material picked in the Details panel has no such
	// independent owner, which is why UAttributeTreeWidget owns those as UPROPERTYs instead.
	FSlateBrush IconBrush;
	bool bHasIcon = false;
};

// Live queries / actions back into the owning UMG widget (which reads the replicated ALevelUnit).
DECLARE_DELEGATE_RetVal_OneParam(int32, FAttrTreeGetNodePoints, FName);
DECLARE_DELEGATE_RetVal(int32, FAttrTreeGetAvailablePoints);
DECLARE_DELEGATE_RetVal_OneParam(bool, FAttrTreeIsUnlocked, FName);
DECLARE_DELEGATE_OneParam(FAttrTreeOnInvest, FName);
DECLARE_DELEGATE(FAttrTreeOnReset);

/**
 * A radial (ring-shaped) attribute tree rendered entirely in Slate.
 *  - Nodes are placed by (Ring, Angle); ring 0 is the centre.
 *  - Links are drawn from each node to its PrevId parent.
 *  - Left-click on a node invests one point.
 *  - Holding the cursor over a node for > TooltipDelaySeconds shows a rich tooltip.
 */
class RTSUNITTEMPLATE_API SAttributeTreeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAttributeTreeWidget)
		: _RingSpacing(90.f)
		, _NodeRadius(26.f)
		, _TooltipDelaySeconds(0.0f)
		, _LockedColor(FLinearColor(0.10f, 0.10f, 0.12f, 1.f))
		, _AvailableColor(FLinearColor(0.20f, 0.65f, 1.00f, 1.f))
		, _PartialColor(FLinearColor(0.95f, 0.75f, 0.15f, 1.f))
		, _FullColor(FLinearColor(0.20f, 0.85f, 0.35f, 1.f))
		, _LinkColor(FLinearColor(0.35f, 0.35f, 0.40f, 1.f))
		, _RingColor(FLinearColor(1.f, 1.f, 1.f, 0.06f))
		, _NodeFontSize(9)
		, _CountFontSize(8)
		, _TooltipTitleFontSize(16)
		, _TooltipBodyFontSize(13)
		, _TooltipMaxWidth(420.f)
		, _TooltipPadding(12.f)
		, _TooltipIconSize(40.f)
		, _TooltipIconGap(8.f)
		, _BackgroundColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.75f))
		, _bFillViewport(true)
		, _BackgroundBrush(nullptr)
		, _BorderBrush(nullptr)
		, _BorderPadding(FMargin(0.f))
	{}
		SLATE_ARGUMENT(float, RingSpacing)
		SLATE_ARGUMENT(float, NodeRadius)
		SLATE_ARGUMENT(float, TooltipDelaySeconds)
		SLATE_ARGUMENT(FLinearColor, LockedColor)
		SLATE_ARGUMENT(FLinearColor, AvailableColor)
		SLATE_ARGUMENT(FLinearColor, PartialColor)
		SLATE_ARGUMENT(FLinearColor, FullColor)
		SLATE_ARGUMENT(FLinearColor, LinkColor)
		SLATE_ARGUMENT(FLinearColor, RingColor)
		SLATE_ARGUMENT(int32, NodeFontSize)
		SLATE_ARGUMENT(int32, CountFontSize)
		SLATE_ARGUMENT(int32, TooltipTitleFontSize)
		SLATE_ARGUMENT(int32, TooltipBodyFontSize)
		SLATE_ARGUMENT(float, TooltipMaxWidth)
		SLATE_ARGUMENT(float, TooltipPadding)
		SLATE_ARGUMENT(float, TooltipIconSize)
		SLATE_ARGUMENT(float, TooltipIconGap)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(bool, bFillViewport)
		/** Pointers INTO the owning UMG wrapper's UPROPERTY brushes - that UPROPERTY is the GC anchor for
		 *  any assigned material/texture. Null = built-in solid fallback / no border.
		 *  Never pass the address of a temporary; the pointee must outlive this widget. */
		SLATE_ARGUMENT(const FSlateBrush*, BackgroundBrush)
		SLATE_ARGUMENT(const FSlateBrush*, BorderBrush)
		SLATE_ARGUMENT(FMargin, BorderPadding)
		SLATE_EVENT(FAttrTreeGetNodePoints, OnGetNodePoints)
		SLATE_EVENT(FAttrTreeGetAvailablePoints, OnGetAvailablePoints)
		SLATE_EVENT(FAttrTreeIsUnlocked, OnIsUnlocked)
		SLATE_EVENT(FAttrTreeOnInvest, OnInvest)
		SLATE_EVENT(FAttrTreeOnReset, OnReset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replace the node model (called whenever the target unit / data table changes). */
	void SetNodes(TArray<FAttributeTreeSlateNode> InNodes);

	/** Re-push panel style from the UMG wrapper (drives Details-panel + runtime live-update).
	 *  Pass null brushes to detach from wrapper-owned memory before the wrapper dies. */
	void SetPanelStyle(const FSlateBrush* InBackgroundBrush, const FSlateBrush* InBorderBrush,
		const FLinearColor& InBackgroundColor, const FMargin& InBorderPadding);

	// --- SWidget interface ---
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	FVector2D GetNodeLocalCenter(const FAttributeTreeSlateNode& Node, const FVector2D& LocalSize) const;
	int32 HitTestNode(const FVector2D& LocalPos, const FVector2D& LocalSize) const;
	int32 GetNodePoints(FName Id) const;
	bool IsUnlocked(FName Id) const;
	void RefreshTooltip();
	FSlateRect ResetButtonRect(const FVector2D& LocalSize) const;

	/** Drives hover detection + timing every frame from the cursor position (reliable under Slate global invalidation). */
	EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);

	/** Draws the hover tooltip directly (no child widget) so it renders reliably. */
	void PaintNodeTooltip(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	/** Draws BorderBrushPtr over the widget rect (inset by BorderPadding). Returns the next free layer,
	 *  unchanged when nothing is drawn. Called from BOTH OnPaint exit paths - the empty-state branch
	 *  early-returns, and that is exactly the state you sit in while authoring the material. */
	int32 PaintPanelBorder(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle) const;

	// Model
	TArray<FAttributeTreeSlateNode> Nodes;
	TMap<FName, int32> IdToIndex;
	int32 MaxRing = 0;

	// Style
	float RingSpacing = 90.f;
	float NodeRadius = 26.f;
	float TooltipDelaySeconds = 0.f;
	FLinearColor LockedColor;
	FLinearColor AvailableColor;
	FLinearColor PartialColor;
	FLinearColor FullColor;
	FLinearColor LinkColor;
	FLinearColor RingColor;
	int32 TooltipTitleFontSize = 16;
	int32 TooltipBodyFontSize = 13;
	float TooltipMaxWidth = 420.f;
	float TooltipPadding = 12.f;
	float TooltipIconSize = 40.f;
	float TooltipIconGap = 8.f;

	// Full-rect dimmed backdrop. PURELY VISUAL: clicks are absorbed by this widget's Visibility (the
	// hit-test grid entry is added in the non-virtual SWidget::Paint, before OnPaint runs) plus
	// OnMouseButtonDown returning Handled - not by this box. Alpha 0 = invisible but still click-eating.
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.04f, 0.75f);
	bool bFillViewport = true;

	// Delegates
	FAttrTreeGetNodePoints OnGetNodePointsDelegate;
	FAttrTreeGetAvailablePoints OnGetAvailablePointsDelegate;
	FAttrTreeIsUnlocked OnIsUnlockedDelegate;
	FAttrTreeOnInvest OnInvestDelegate;
	FAttrTreeOnReset OnResetDelegate;

	// Render resources
	FSlateBrush NodeBackgroundBrush;
	// Owned fallback. Procedural white FSlateColorBrush - its ResourceObject is always null, so it is
	// GC-inert. Used whenever BackgroundBrushPtr is null (raw SNew, or after the wrapper detached us).
	FSlateBrush BackgroundBrush;
	// Non-owning views into the UMG wrapper's UPROPERTY brushes (that UPROPERTY is the GC anchor for any
	// assigned material/texture). Nulled by the wrapper in ReleaseSlateResources so they cannot dangle.
	const FSlateBrush* BackgroundBrushPtr = nullptr;
	const FSlateBrush* BorderBrushPtr = nullptr;
	FMargin BorderPadding = FMargin(0.f);
	FSlateFontInfo NodeFont;
	FSlateFontInfo CountFont;

	// Hover / tooltip
	int32 HoveredNodeIndex = INDEX_NONE;
	float HoverElapsed = 0.f;
	FVector2D LocalMousePos = FVector2D::ZeroVector;
	mutable FGeometry CachedGeometry;

	// Pan (drag the whole tree when it is larger than the view)
	FVector2D PanOffset = FVector2D::ZeroVector;
	bool bIsPanning = false;
	FVector2D PanMouseStart = FVector2D::ZeroVector;
	FVector2D PanOffsetStart = FVector2D::ZeroVector;

	// Tooltip widgets
	TSharedPtr<SBox> TooltipContainer;
	TSharedPtr<STextBlock> TooltipTitle;
	TSharedPtr<STextBlock> TooltipBody;
	TSharedPtr<STextBlock> TooltipPoints;
	TSharedPtr<SImage> TooltipIcon;
	FSlateBrush TooltipIconBrush;
};
