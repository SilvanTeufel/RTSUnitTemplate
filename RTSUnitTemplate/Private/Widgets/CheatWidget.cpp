// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/CheatWidget.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"
#include "Core/WorkerData.h"
#include "Controller/PlayerController/WidgetController.h"

#define LOCTEXT_NAMESPACE "CheatWidget"

UCheatWidget::UCheatWidget()
{
	bIsVariable = true;
	SetVisibility(ESlateVisibility::Visible);
}

AWidgetController* UCheatWidget::GetLocalWidgetController() const
{
	// Der HUD gehoert immer dem lokalen Spieler; der erste Controller ist hier der richtige.
	if (const UWorld* World = GetWorld())
	{
		return Cast<AWidgetController>(World->GetFirstPlayerController());
	}
	return nullptr;
}

void UCheatWidget::FillResources()
{
	AWidgetController* PC = GetLocalWidgetController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Ressourcen nicht gefuellt: kein WidgetController"));
		return;
	}

	// Ueber den Controller, nicht direkt ueber den GameMode: ModifyResource dort ist ein
	// Server-RPC. Auf einem Client gibt es den GameMode gar nicht, ein Direktaufruf waere
	// wirkungslos - und zwar lautlos.
	const int32 TeamId = PC->SelectableTeamId;
	for (uint8 i = 0; i < static_cast<uint8>(EResourceType::MAX); ++i)
	{
		PC->ModifyResource(static_cast<EResourceType>(i), TeamId, ResourceFillAmount);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Ressourcen +%.0f je Art fuer Team %d"),
		ResourceFillAmount, TeamId);
}

void UCheatWidget::WinCurrentLevel()
{
	AWidgetController* PC = GetLocalWidgetController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Sieg nicht ausloesbar: kein WidgetController"));
		return;
	}
	PC->Server_CheatWinCurrentLevel();
}

FReply UCheatWidget::HandleToggleClicked()
{
	bPanelOpen = !bPanelOpen;
	return FReply::Handled();
}

FReply UCheatWidget::HandleFillClicked()
{
	FillResources();
	return FReply::Handled();
}

FReply UCheatWidget::HandleWinClicked()
{
	WinCurrentLevel();
	return FReply::Handled();
}

EVisibility UCheatWidget::GetPanelVisibility() const
{
	return bPanelOpen ? EVisibility::Visible : EVisibility::Collapsed;
}

FText UCheatWidget::GetToggleLabel() const
{
	return bPanelOpen ? LOCTEXT("CheatClose", "CHEATS  ▲")
	                  : LOCTEXT("CheatOpen", "CHEATS  ▼");
}

TSharedRef<SWidget> UCheatWidget::RebuildWidget()
{
	bPanelOpen = bStartOpen;

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);

	auto MacheKnopf = [this, Font](const FText& Beschriftung, FOnClicked BeiKlick)
	{
		return SNew(SButton)
			.OnClicked(BeiKlick)
			.ButtonColorAndOpacity(PanelColor)
			.ContentPadding(FMargin(10.f, 5.f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Beschriftung)
				.Font(Font)
				.ColorAndOpacity(FSlateColor(AccentColor))
			];
	};

	MyRoot =
		SNew(SVerticalBox)

		// Der schmale Knopf, der immer sichtbar ist.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.OnClicked(FOnClicked::CreateUObject(this, &UCheatWidget::HandleToggleClicked))
			.ButtonColorAndOpacity(PanelColor)
			.ContentPadding(FMargin(10.f, 4.f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(
					TAttribute<FText>::FGetter::CreateUObject(this, &UCheatWidget::GetToggleLabel)))
				.Font(Font)
				.ColorAndOpacity(FSlateColor(AccentColor))
			]
		]

		// Das ausklappbare Panel.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(PanelColor)
			.Padding(FMargin(6.f))
			.Visibility(TAttribute<EVisibility>::Create(
				TAttribute<EVisibility>::FGetter::CreateUObject(this, &UCheatWidget::GetPanelVisibility)))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 4.f)
				[
					MacheKnopf(LOCTEXT("CheatFill", "Ressourcen fuellen"),
						FOnClicked::CreateUObject(this, &UCheatWidget::HandleFillClicked))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MacheKnopf(LOCTEXT("CheatWin", "Level gewinnen"),
						FOnClicked::CreateUObject(this, &UCheatWidget::HandleWinClicked))
				]
			]
		];

	return MyRoot.ToSharedRef();
}

void UCheatWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyRoot.Reset();
}

#if WITH_EDITOR
const FText UCheatWidget::GetPaletteCategory()
{
	return LOCTEXT("CheatPalette", "RTSUnitTemplate");
}
#endif

#undef LOCTEXT_NAMESPACE
