// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/TutorialWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TutorialWidget"

UTutorialWidget::UTutorialWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A hint, never in the way: clicks and selection boxes go through to the game - only the close button takes them.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TextFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);
	HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	HeaderText = LOCTEXT("Header", "TUTORIAL");
	CloseButtonText = LOCTEXT("Close", "X");
	CloseButtonStyle = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.12f)))
		.SetPressed(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.22f)))
		.SetNormalPadding(FMargin(6.f, 0.f))
		.SetPressedPadding(FMargin(6.f, 1.f, 6.f, 0.f));
}

TSharedRef<SWidget> UTutorialWidget::RebuildWidget()
{
	const bool bHasFrame = FrameBrush.GetResourceObject() != nullptr;
	const bool bHasHeader = !HeaderText.IsEmpty() || bShowStepCounter || bShowCloseButton;

	TSharedRef<SWidget> Root =
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, TopOffset, 0.f, 0.f))
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SAssignNew(PanelBox, SBox)
			.WidthOverride(PanelWidth)
			[
				SNew(SOverlay)
				.Visibility(EVisibility::SelfHitTestInvisible)
				+ SOverlay::Slot()
				[
					// Without a frame material a dim plate, so the text stays readable over the map.
					SNew(SImage)
					.Visibility(EVisibility::HitTestInvisible)
					.Image(bHasFrame ? &FrameBrush : FAppStyle::GetBrush("WhiteBrush"))
					.ColorAndOpacity(bHasFrame ? FLinearColor::White : FLinearColor(0.f, 0.003f, 0.005f, 0.75f))
				]
				+ SOverlay::Slot()
				.Padding(ContentPadding)
				[
					SNew(SVerticalBox)
					.Visibility(EVisibility::SelfHitTestInvisible)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility(bHasHeader ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(HeaderText)
							.Font(HeaderFont)
							.ColorAndOpacity(HeaderColor)
							.Visibility(EVisibility::HitTestInvisible)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SAssignNew(CounterBlock, STextBlock)
							.Text(MakeCounterText())
							.Font(HeaderFont)
							.ColorAndOpacity(HeaderColor)
							.Visibility(bShowStepCounter ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(14.f, 0.f, 0.f, 0.f)
						[
							// The only part that takes the mouse. Not focusable, so the game keeps its hotkeys.
							SNew(SButton)
							.ButtonStyle(&CloseButtonStyle)
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("CloseTip", "Close the tutorial"))
							.OnClicked(FOnClicked::CreateUObject(this, &UTutorialWidget::HandleCloseClicked))
							.Visibility(bShowCloseButton ? EVisibility::Visible : EVisibility::Collapsed)
							[
								SNew(STextBlock)
								.Text(CloseButtonText)
								.Font(HeaderFont)
								.ColorAndOpacity(HeaderColor)
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 6.f)
					[
						SNew(SImage)
						.Image(&HeaderBrush)
						.Visibility(HeaderBrush.GetResourceObject() ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, bHasHeader ? 4.f : 0.f, 0.f, 0.f)
					[
						SAssignNew(TextBlock, STextBlock)
						.Text(IsDesignTime() && CurrentText.IsEmpty() ? LOCTEXT("Preview", "Select a worker with the mouse, or press 1 if nothing is selected.") : CurrentText)
						.Font(TextFont)
						.ColorAndOpacity(TextColor)
						.AutoWrapText(true)
						.Justification(ETextJustify::Center)
						.Visibility(EVisibility::HitTestInvisible)
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(&OverlayBrush)
					.Visibility(OverlayBrush.GetResourceObject() ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				]
			]
		];

	if (IsDesignTime())
	{
		Opacity = 1.f;
	}
	ApplyOpacity();
	return Root;
}

void UTutorialWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PanelBox.Reset();
	TextBlock.Reset();
	CounterBlock.Reset();
}

void UTutorialWidget::ShowStep(const FText& Text, int32 Index, int32 Count)
{
	StepCount = Count;
	if (Opacity <= 0.f)
	{
		// Nothing on screen yet: straight in.
		CurrentText = Text;
		StepIndex = Index;
		bHasPending = false;
		if (TextBlock.IsValid()) TextBlock->SetText(CurrentText);
		if (CounterBlock.IsValid()) CounterBlock->SetText(MakeCounterText());
	}
	else
	{
		PendingText = Text;
		PendingIndex = Index;
		bHasPending = true;
	}
	bWantVisible = true;
}

FReply UTutorialWidget::HandleCloseClicked()
{
	HideTutorial();
	OnCloseClicked.Broadcast();
	return FReply::Handled();
}

void UTutorialWidget::HideTutorial()
{
	bHasPending = false;
	bWantVisible = false;
}

void UTutorialWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Step = InDeltaTime / FMath::Max(FadeTime, 0.01f);
	const float OldOpacity = Opacity;
	if (bHasPending || !bWantVisible)
	{
		Opacity = FMath::Max(Opacity - Step, 0.f);
		if (bHasPending && Opacity <= 0.f)
		{
			// Faded out: swap the text, then fade the new one in.
			CurrentText = PendingText;
			StepIndex = PendingIndex;
			bHasPending = false;
			if (TextBlock.IsValid()) TextBlock->SetText(CurrentText);
			if (CounterBlock.IsValid()) CounterBlock->SetText(MakeCounterText());
		}
	}
	else
	{
		Opacity = FMath::Min(Opacity + Step, 1.f);
	}
	if (Opacity != OldOpacity)
	{
		ApplyOpacity();
	}
}

void UTutorialWidget::ApplyOpacity()
{
	if (!PanelBox.IsValid())
	{
		return;
	}
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Opacity, 2.f);
	PanelBox->SetRenderOpacity(Eased);
	PanelBox->SetRenderTransform(FSlateRenderTransform(FVector2f(0.f, -(1.f - Eased) * SlideDistance)));
	PanelBox->SetVisibility(Opacity > 0.f ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
}

FText UTutorialWidget::MakeCounterText() const
{
	return StepCount > 0 ? FText::Format(LOCTEXT("Counter", "{0} / {1}"), FText::AsNumber(StepIndex + 1), FText::AsNumber(StepCount)) : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
