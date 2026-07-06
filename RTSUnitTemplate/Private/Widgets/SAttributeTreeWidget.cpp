// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/SAttributeTreeWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

void SAttributeTreeWidget::Construct(const FArguments& InArgs)
{
	RingSpacing         = InArgs._RingSpacing;
	NodeRadius          = InArgs._NodeRadius;
	TooltipDelaySeconds = InArgs._TooltipDelaySeconds;
	LockedColor         = InArgs._LockedColor;
	AvailableColor      = InArgs._AvailableColor;
	PartialColor        = InArgs._PartialColor;
	FullColor           = InArgs._FullColor;
	LinkColor           = InArgs._LinkColor;
	RingColor           = InArgs._RingColor;
	TooltipTitleFontSize= InArgs._TooltipTitleFontSize;
	TooltipBodyFontSize = InArgs._TooltipBodyFontSize;
	TooltipMaxWidth     = InArgs._TooltipMaxWidth;
	TooltipPadding      = InArgs._TooltipPadding;
	TooltipIconSize     = InArgs._TooltipIconSize;
	TooltipIconGap      = InArgs._TooltipIconGap;

	OnGetNodePointsDelegate     = InArgs._OnGetNodePoints;
	OnGetAvailablePointsDelegate= InArgs._OnGetAvailablePoints;
	OnIsUnlockedDelegate        = InArgs._OnIsUnlocked;
	OnInvestDelegate            = InArgs._OnInvest;
	OnResetDelegate             = InArgs._OnReset;

	NodeBackgroundBrush = FSlateRoundedBoxBrush(FLinearColor::White, NodeRadius, FVector2D(NodeRadius * 2.f, NodeRadius * 2.f));

	NodeFont  = FCoreStyle::GetDefaultFontStyle("Bold", InArgs._NodeFontSize);
	CountFont = FCoreStyle::GetDefaultFontStyle("Bold", InArgs._CountFontSize);

	SetCanTick(true);
	// Tick() is unreliable under Slate global invalidation; an active timer always runs.
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAttributeTreeWidget::HandleActiveTimer));

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SAssignNew(TooltipContainer, SBox)
			.Visibility(EVisibility::Collapsed)
			.MaxDesiredWidth(320.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(8.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.f, 0.f, 6.f, 0.f)
						[
							SAssignNew(TooltipIcon, SImage)
							.Image(&TooltipIconBrush)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SAssignNew(TooltipTitle, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 0.f)
					[
						SAssignNew(TooltipBody, STextBlock)
						.AutoWrapText(true)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 0.f)
					[
						SAssignNew(TooltipPoints, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 1.f, 1.f)))
					]
				]
			]
		]
	];
}

void SAttributeTreeWidget::SetNodes(TArray<FAttributeTreeSlateNode> InNodes)
{
	Nodes = MoveTemp(InNodes);
	IdToIndex.Reset();
	MaxRing = 0;
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		IdToIndex.Add(Nodes[i].Id, i);
		MaxRing = FMath::Max(MaxRing, Nodes[i].Ring);
	}
	HoveredNodeIndex = INDEX_NONE;
	HoverElapsed = 0.f;
	PanOffset = FVector2D::ZeroVector;
	bIsPanning = false;
	if (TooltipContainer.IsValid())
	{
		TooltipContainer->SetVisibility(EVisibility::Collapsed);
	}
	Invalidate(EInvalidateWidgetReason::Layout);
}

FVector2D SAttributeTreeWidget::GetNodeLocalCenter(const FAttributeTreeSlateNode& Node, const FVector2D& LocalSize) const
{
	const FVector2D Center = LocalSize * 0.5f + PanOffset;
	const float R = (float)Node.Ring * RingSpacing;
	const float A = FMath::DegreesToRadians(Node.Angle);
	return Center + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R;
}

int32 SAttributeTreeWidget::HitTestNode(const FVector2D& LocalPos, const FVector2D& LocalSize) const
{
	const float HitRadiusSq = FMath::Square(NodeRadius + 2.f);
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		const FVector2D C = GetNodeLocalCenter(Nodes[i], LocalSize);
		if (FVector2D::DistSquared(LocalPos, C) <= HitRadiusSq)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 SAttributeTreeWidget::GetNodePoints(FName Id) const
{
	return OnGetNodePointsDelegate.IsBound() ? OnGetNodePointsDelegate.Execute(Id) : 0;
}

FSlateRect SAttributeTreeWidget::ResetButtonRect(const FVector2D& LocalSize) const
{
	// Fixed screen-space button in the top-left corner (not affected by pan).
	const float W = 96.f, H = 26.f, M = 10.f;
	return FSlateRect(M, M, M + W, M + H);
}

bool SAttributeTreeWidget::IsUnlocked(FName Id) const
{
	return OnIsUnlockedDelegate.IsBound() ? OnIsUnlockedDelegate.Execute(Id) : true;
}

FVector2D SAttributeTreeWidget::ComputeDesiredSize(float) const
{
	const float R = (float)FMath::Max(MaxRing, 1) * RingSpacing + NodeRadius + 12.f;
	return FVector2D(R * 2.f, R * 2.f);
}

int32 SAttributeTreeWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	CachedGeometry = AllottedGeometry;
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = LocalSize * 0.5f + PanOffset;
	const FPaintGeometry WholeGeom = AllottedGeometry.ToPaintGeometry();

	int32 Layer = LayerId;

	// --- 0. Empty-state placeholder ---
	if (Nodes.Num() == 0)
	{
		const int32 Segments = 72;
		const float R = RingSpacing;
		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		for (int32 s = 0; s <= Segments; ++s)
		{
			const float A = (2.f * PI * (float)s) / (float)Segments;
			Points.Add(Center + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, WholeGeom, Points, ESlateDrawEffect::None, AvailableColor, true, 2.f);

		const float Size = NodeRadius * 2.f;
		const FVector2D TopLeft = Center - FVector2D(NodeRadius, NodeRadius);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(Size, Size), FSlateLayoutTransform(1.f, TopLeft)),
			&NodeBackgroundBrush, ESlateDrawEffect::None, LockedColor);

		const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FString Msg = TEXT("AttributeTree: kein DataTable zugewiesen");
		const FVector2D TextSize = FM->Measure(Msg, NodeFont);
		FSlateDrawElement::MakeText(OutDrawElements, Layer + 2,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(1.f, Center + FVector2D(-TextSize.X * 0.5f, NodeRadius + 6.f))),
			Msg, NodeFont, ESlateDrawEffect::None, FLinearColor::White);

		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Layer + 3, InWidgetStyle, bParentEnabled);
	}

	// --- 1. Faint ring guide circles ---
	{
		const int32 Segments = 72;
		for (int32 Ring = 1; Ring <= MaxRing; ++Ring)
		{
			const float R = (float)Ring * RingSpacing;
			TArray<FVector2D> Points;
			Points.Reserve(Segments + 1);
			for (int32 s = 0; s <= Segments; ++s)
			{
				const float A = (2.f * PI * (float)s) / (float)Segments;
				Points.Add(Center + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R);
			}
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, WholeGeom, Points, ESlateDrawEffect::None, RingColor, true, 1.5f);
		}
	}
	++Layer;

	// --- 2. Links (child -> parent) ---
	{
		for (const FAttributeTreeSlateNode& Node : Nodes)
		{
			if (Node.PrevId.IsNone())
			{
				continue;
			}
			const int32* ParentIdx = IdToIndex.Find(Node.PrevId);
			if (!ParentIdx || !Nodes.IsValidIndex(*ParentIdx))
			{
				continue;
			}
			const FVector2D ChildC = GetNodeLocalCenter(Node, LocalSize);
			const FVector2D ParentC = GetNodeLocalCenter(Nodes[*ParentIdx], LocalSize);

			const int32 Pts = GetNodePoints(Node.Id);
			FLinearColor C = LinkColor;
			if (Pts >= FMath::Max(1, Node.MaxPoints)) { C = FullColor; }
			else if (IsUnlocked(Node.Id))            { C = LinkColor * 1.8f; C.A = 1.f; }

			TArray<FVector2D> Line;
			Line.Add(ParentC);
			Line.Add(ChildC);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, WholeGeom, Line, ESlateDrawEffect::None, C, true, 3.f);
		}
	}
	++Layer;

	// --- 3. Nodes ---
	const int32 NodeLayer = Layer;
	const int32 IconLayer = Layer + 1;
	const int32 TextLayer = Layer + 2;
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	for (const FAttributeTreeSlateNode& Node : Nodes)
	{
		const FVector2D C = GetNodeLocalCenter(Node, LocalSize);
		const int32 Pts = GetNodePoints(Node.Id);
		const int32 MaxPts = FMath::Max(1, Node.MaxPoints);
		const bool bUnlocked = IsUnlocked(Node.Id);

		FLinearColor StateColor;
		if (!bUnlocked)            StateColor = LockedColor;
		else if (Pts <= 0)         StateColor = AvailableColor;
		else if (Pts >= MaxPts)    StateColor = FullColor;
		else                       StateColor = PartialColor;

		// Outer rim (branch identity colour).
		{
			const float Size = NodeRadius * 2.f;
			const FVector2D TopLeft = C - FVector2D(NodeRadius, NodeRadius);
			FSlateDrawElement::MakeBox(OutDrawElements, NodeLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(Size, Size), FSlateLayoutTransform(1.f, TopLeft)),
				&NodeBackgroundBrush, ESlateDrawEffect::None, Node.BaseColor);
		}
		// Inner disc (state colour).
		{
			const float Inner = FMath::Max(4.f, NodeRadius - 3.f);
			const float Size = Inner * 2.f;
			const FVector2D TopLeft = C - FVector2D(Inner, Inner);
			FSlateDrawElement::MakeBox(OutDrawElements, NodeLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(Size, Size), FSlateLayoutTransform(1.f, TopLeft)),
				&NodeBackgroundBrush, ESlateDrawEffect::None, StateColor);
		}

		// Icon (dim when locked).
		if (Node.bHasIcon)
		{
			const float IconSize = FMath::Max(6.f, (NodeRadius - 6.f) * 2.f);
			const FVector2D TopLeft = C - FVector2D(IconSize * 0.5f, IconSize * 0.5f);
			const FLinearColor IconTint = bUnlocked ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
			FSlateDrawElement::MakeBox(OutDrawElements, IconLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(IconSize, IconSize), FSlateLayoutTransform(1.f, TopLeft)),
				&Node.IconBrush, ESlateDrawEffect::None, IconTint);
		}

		// Points count "x/max".
		{
			const FString CountStr = FString::Printf(TEXT("%d/%d"), Pts, MaxPts);
			const FVector2D TextSize = FontMeasure->Measure(CountStr, CountFont);
			const FVector2D TextPos = C + FVector2D(-TextSize.X * 0.5f, NodeRadius - 2.f);
			FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
				AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(1.f, TextPos)),
				CountStr, CountFont, ESlateDrawEffect::None, FLinearColor::White);
		}
	}

	Layer = TextLayer + 1;

	// --- 3b. Reset button (fixed top-left, not panned) ---
	if (OnResetDelegate.IsBound())
	{
		const FSlateRect R = ResetButtonRect(LocalSize);
		const FVector2D BtnSize(R.GetSize().X, R.GetSize().Y);
		const FVector2D BtnTL(R.Left, R.Top);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(BtnSize, FSlateLayoutTransform(1.f, BtnTL)),
			&NodeBackgroundBrush, ESlateDrawEffect::None, FLinearColor(0.55f, 0.16f, 0.16f, 0.95f));

		const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FString Msg = TEXT("Reset");
		const FVector2D TS = FM->Measure(Msg, NodeFont);
		FSlateDrawElement::MakeText(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(TS, FSlateLayoutTransform(1.f, FVector2D(R.Left + (BtnSize.X - TS.X) * 0.5f, R.Top + (BtnSize.Y - TS.Y) * 0.5f))),
			Msg, NodeFont, ESlateDrawEffect::None, FLinearColor::White);
		Layer += 2;
	}

	// --- 4. Hover tooltip (drawn directly, on top) ---
	PaintNodeTooltip(AllottedGeometry, OutDrawElements, Layer);
	Layer += 8;

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Layer, InWidgetStyle, bParentEnabled);
}

void SAttributeTreeWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Hover timing is driven by HandleActiveTimer (Tick can be skipped under invalidation).
}

EActiveTimerReturnType SAttributeTreeWidget::HandleActiveTimer(double InCurrentTime, float InDeltaTime)
{
	// Accumulate hover time against the node set by OnMouseMove, then repaint (drives the tooltip
	// reliably even when Tick is skipped under Slate global invalidation).
	if (!bIsPanning && Nodes.IsValidIndex(HoveredNodeIndex))
	{
		HoverElapsed += InDeltaTime;
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

static TArray<FString> WrapTooltipText(const FString& Text, const FSlateFontInfo& Font, float MaxWidth, const TSharedRef<FSlateFontMeasure>& FM)
{
	TArray<FString> Lines;
	TArray<FString> Paras;
	Text.ParseIntoArray(Paras, TEXT("\n"), false);
	for (const FString& Para : Paras)
	{
		TArray<FString> Words;
		Para.ParseIntoArray(Words, TEXT(" "), true);
		if (Words.Num() == 0) { Lines.Add(TEXT("")); continue; }
		FString Cur;
		for (const FString& W : Words)
		{
			const FString Test = Cur.IsEmpty() ? W : Cur + TEXT(" ") + W;
			if (FM->Measure(Test, Font).X <= MaxWidth) { Cur = Test; }
			else { if (!Cur.IsEmpty()) { Lines.Add(Cur); } Cur = W; }
		}
		Lines.Add(Cur);
	}
	return Lines;
}

void SAttributeTreeWidget::PaintNodeTooltip(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (!Nodes.IsValidIndex(HoveredNodeIndex) || HoverElapsed < TooltipDelaySeconds)
	{
		return;
	}
	const FAttributeTreeSlateNode& Node = Nodes[HoveredNodeIndex];
	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", TooltipTitleFontSize);
	const FSlateFontInfo BodyFont  = FCoreStyle::GetDefaultFontStyle("Regular", TooltipBodyFontSize);

	const float Pad = TooltipPadding;
	const float MaxTextW = TooltipMaxWidth;
	const float IconSz  = Node.bHasIcon ? TooltipIconSize : 0.f;
	const float IconGap = Node.bHasIcon ? TooltipIconGap : 0.f;

	const FString Title  = (Node.ToolTipTitle.IsEmpty() ? Node.DisplayName : Node.ToolTipTitle).ToString();
	const FString Points = FString::Printf(TEXT("Points: %d / %d"), GetNodePoints(Node.Id), FMath::Max(1, Node.MaxPoints));
	const TArray<FString> BodyLines = Node.ToolTipText.IsEmpty() ? TArray<FString>() : WrapTooltipText(Node.ToolTipText.ToString(), BodyFont, MaxTextW, FM);

	const FVector2D TitleSz  = FM->Measure(Title, TitleFont);
	const FVector2D PointsSz = FM->Measure(Points, BodyFont);
	const float BodyLineH = (float)FM->GetMaxCharacterHeight(BodyFont) + 2.f;
	float TextW = FMath::Max((float)TitleSz.X, (float)PointsSz.X);
	for (const FString& L : BodyLines) { TextW = FMath::Max(TextW, (float)FM->Measure(L, BodyFont).X); }
	TextW = FMath::Min(TextW, MaxTextW);

	const float HeaderH = FMath::Max((float)TitleSz.Y, IconSz);
	const float BoxW = Pad * 2.f + IconSz + IconGap + TextW;
	const float BoxH = Pad * 2.f + HeaderH + (BodyLines.Num() > 0 ? (6.f + BodyLines.Num() * BodyLineH) : 0.f) + 6.f + (float)PointsSz.Y;

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	FVector2D Pos = LocalMousePos + FVector2D(18.f, 18.f);
	Pos.X = FMath::Clamp(Pos.X, 2.f, FMath::Max(2.f, Size.X - BoxW - 2.f));
	Pos.Y = FMath::Clamp(Pos.Y, 2.f, FMath::Max(2.f, Size.Y - BoxH - 2.f));

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(BoxW, BoxH), FSlateLayoutTransform(1.f, Pos)),
		&NodeBackgroundBrush, ESlateDrawEffect::None, FLinearColor(0.03f, 0.03f, 0.05f, 0.97f));
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2D(4.f, BoxH), FSlateLayoutTransform(1.f, Pos)),
		&NodeBackgroundBrush, ESlateDrawEffect::None, Node.BaseColor);

	const float CurX = Pos.X + Pad;
	float CurY = Pos.Y + Pad;
	if (Node.bHasIcon)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(IconSz, IconSz), FSlateLayoutTransform(1.f, FVector2D(CurX, CurY))),
			&Node.IconBrush, ESlateDrawEffect::None, FLinearColor::White);
	}
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
		AllottedGeometry.ToPaintGeometry(TitleSz, FSlateLayoutTransform(1.f, FVector2D(CurX + IconSz + IconGap, CurY + (HeaderH - TitleSz.Y) * 0.5f))),
		Title, TitleFont, ESlateDrawEffect::None, FLinearColor::White);
	CurY += HeaderH + (BodyLines.Num() > 0 ? 6.f : 0.f);
	for (const FString& L : BodyLines)
	{
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(TextW, BodyLineH), FSlateLayoutTransform(1.f, FVector2D(Pos.X + Pad, CurY))),
			L, BodyFont, ESlateDrawEffect::None, FLinearColor(0.85f, 0.85f, 0.88f, 1.f));
		CurY += BodyLineH;
	}
	CurY += 6.f;
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
		AllottedGeometry.ToPaintGeometry(PointsSz, FSlateLayoutTransform(1.f, FVector2D(Pos.X + Pad, CurY))),
		Points, BodyFont, ESlateDrawEffect::None, FLinearColor(0.55f, 0.8f, 1.f, 1.f));
}

void SAttributeTreeWidget::RefreshTooltip()
{
	if (!TooltipContainer.IsValid())
	{
		return;
	}

	if (!Nodes.IsValidIndex(HoveredNodeIndex) || HoverElapsed < TooltipDelaySeconds)
	{
		TooltipContainer->SetVisibility(EVisibility::Collapsed);
		return;
	}

	const FAttributeTreeSlateNode& Node = Nodes[HoveredNodeIndex];

	if (TooltipTitle.IsValid())
	{
		const FText Title = Node.ToolTipTitle.IsEmpty() ? Node.DisplayName : Node.ToolTipTitle;
		TooltipTitle->SetText(Title);
	}
	if (TooltipBody.IsValid())
	{
		TooltipBody->SetText(Node.ToolTipText);
		TooltipBody->SetVisibility(Node.ToolTipText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	}
	if (TooltipPoints.IsValid())
	{
		const int32 Pts = GetNodePoints(Node.Id);
		TooltipPoints->SetText(FText::FromString(FString::Printf(TEXT("Points: %d / %d"), Pts, FMath::Max(1, Node.MaxPoints))));
	}
	if (TooltipIcon.IsValid())
	{
		if (Node.bHasIcon)
		{
			TooltipIconBrush = Node.IconBrush;
			TooltipIconBrush.ImageSize = FVector2D(28.f, 28.f);
			TooltipIcon->SetVisibility(EVisibility::HitTestInvisible);
		}
		else
		{
			TooltipIcon->SetVisibility(EVisibility::Collapsed);
		}
	}

	TooltipContainer->SetRenderTransform(FSlateRenderTransform(FVector2f((float)LocalMousePos.X + 18.f, (float)LocalMousePos.Y + 18.f)));
	TooltipContainer->SetVisibility(EVisibility::HitTestInvisible);
}

FReply SAttributeTreeWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		// Reset button (fixed top-left) takes priority over nodes/pan.
		if (OnResetDelegate.IsBound() && ResetButtonRect(MyGeometry.GetLocalSize()).ContainsPoint(Local))
		{
			OnResetDelegate.Execute();
			return FReply::Handled();
		}

		const int32 Idx = HitTestNode(Local, MyGeometry.GetLocalSize());
		if (Nodes.IsValidIndex(Idx))
		{
			OnInvestDelegate.ExecuteIfBound(Nodes[Idx].Id);
			return FReply::Handled();
		}

		// Empty space -> start dragging (pan) the whole tree.
		bIsPanning = true;
		PanMouseStart = Local;
		PanOffsetStart = PanOffset;
		HoveredNodeIndex = INDEX_NONE;
		if (TooltipContainer.IsValid())
		{
			TooltipContainer->SetVisibility(EVisibility::Collapsed);
		}
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SAttributeTreeWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsPanning && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SAttributeTreeWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	LocalMousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (bIsPanning)
	{
		PanOffset = PanOffsetStart + (LocalMousePos - PanMouseStart);
		return FReply::Handled();
	}
	const int32 Idx = HitTestNode(LocalMousePos, MyGeometry.GetLocalSize());
	if (Idx != HoveredNodeIndex)
	{
		HoveredNodeIndex = Idx;
		HoverElapsed = 0.f;
	}
	return FReply::Unhandled();
}

void SAttributeTreeWidget::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	HoveredNodeIndex = INDEX_NONE;
	HoverElapsed = 0.f;
	if (TooltipContainer.IsValid())
	{
		TooltipContainer->SetVisibility(EVisibility::Collapsed);
	}
}
