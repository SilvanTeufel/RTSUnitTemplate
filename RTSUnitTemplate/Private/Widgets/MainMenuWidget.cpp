// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/MainMenuWidget.h"

#include "Controller/PlayerController/CameraControllerBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "System/GameSaveSubsystem.h"
#include "System/MapSwitchSubsystem.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SaveGameWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FSlateBrush MakeColorBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	// Remembered theme, GameUserSettings.ini - survives map changes and restarts.
	const TCHAR* ThemeConfigSection = TEXT("/Script/RTSUnitTemplate.MainMenuWidget");
	const TCHAR* ThemeConfigKey = TEXT("Theme");

	void ApplySounds(FButtonStyle& Style, USoundBase* Hover, USoundBase* Click)
	{
		if (Hover)
		{
			FSlateSound Sound;
			Sound.SetResourceObject(Hover);
			Style.SetHoveredSound(Sound);
		}
		if (Click)
		{
			FSlateSound Sound;
			Sound.SetResourceObject(Click);
			Style.SetPressedSound(Sound);
		}
	}
}

FMainMenuStyle::FMainMenuStyle()
{
	Background = MakeColorBrush(FLinearColor(0.01f, 0.015f, 0.025f));
	Shade = MakeColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Panel = MakeColorBrush(FLinearColor(0.01f, 0.03f, 0.05f, 0.7f));
	ButtonNormal = MakeColorBrush(FLinearColor(0.02f, 0.08f, 0.13f, 0.85f));
	ButtonHovered = MakeColorBrush(FLinearColor(0.05f, 0.22f, 0.34f, 0.95f));
	ButtonPressed = MakeColorBrush(FLinearColor(0.01f, 0.05f, 0.08f, 1.f));
	ButtonDisabled = MakeColorBrush(FLinearColor(0.03f, 0.035f, 0.04f, 0.6f));

	TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 46);
	SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);
	ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
}

FMainMenuTheme::FMainMenuTheme()
{
	const FMainMenuStyle Defaults;
	Panel = Defaults.Panel;
	ButtonNormal = Defaults.ButtonNormal;
	ButtonHovered = Defaults.ButtonHovered;
	ButtonPressed = Defaults.ButtonPressed;
	ButtonDisabled = Defaults.ButtonDisabled;
}

void UMainMenuWidget::SetupMenu(const FMainMenuConfig& InConfig)
{
	Config = InConfig;

	int32 Saved = 0;
	if (GConfig)
	{
		GConfig->GetInt(ThemeConfigSection, ThemeConfigKey, Saved, GGameUserSettingsIni);
	}
	ThemeIndex = Config.Themes.IsValidIndex(Saved) ? Saved : 0;
	ApplyTheme();

	if (RootHost.IsValid())
	{
		RootHost->SetContent(BuildRoot());
	}
}

void UMainMenuWidget::SetTheme(int32 Index)
{
	if (!Config.Themes.IsValidIndex(Index))
	{
		return;
	}

	ThemeIndex = Index;
	if (GConfig)
	{
		GConfig->SetInt(ThemeConfigSection, ThemeConfigKey, ThemeIndex, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	ApplyTheme();
	if (RootHost.IsValid())
	{
		RootHost->SetContent(BuildRoot());
	}
}

void UMainMenuWidget::ApplyTheme()
{
	ActiveStyle = Config.Style;
	if (!Config.Themes.IsValidIndex(ThemeIndex))
	{
		return;
	}

	const FMainMenuTheme& Theme = Config.Themes[ThemeIndex];
	ActiveStyle.Panel = Theme.Panel;
	ActiveStyle.ButtonNormal = Theme.ButtonNormal;
	ActiveStyle.ButtonHovered = Theme.ButtonHovered;
	ActiveStyle.ButtonPressed = Theme.ButtonPressed;
	ActiveStyle.ButtonDisabled = Theme.ButtonDisabled;
	ActiveStyle.TitleColor = Theme.TitleColor;
	ActiveStyle.AccentColor = Theme.AccentColor;
}

void UMainMenuWidget::RefreshMenu()
{
	// Not on screen yet: RebuildWidget builds the panel when it gets there.
	if (PanelHost.IsValid())
	{
		PanelHost->SetContent(BuildPanel());
	}
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Coming back to the menu (a popup closed, the widget was re-added): unlocks may have changed.
	bTravelling = false;
	RefreshMenu();
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	ApplyTheme();
	SAssignNew(RootHost, SBox);
	RootHost->SetContent(BuildRoot());
	return RootHost.ToSharedRef();
}

TSharedRef<SWidget> UMainMenuWidget::BuildRoot()
{
	const FMainMenuStyle& Style = ActiveStyle;

	ThemeButtonStyle = FButtonStyle()
		.SetNormal(Style.ButtonNormal)
		.SetHovered(Style.ButtonHovered)
		.SetPressed(Style.ButtonPressed)
		.SetDisabled(Style.ButtonDisabled)
		.SetNormalPadding(FMargin(18.f, 6.f))
		.SetPressedPadding(FMargin(18.f, 7.f, 18.f, 5.f));
	ApplySounds(ThemeButtonStyle, Style.HoverSound, Style.ClickSound);

	TSharedRef<SOverlay> Root = SNew(SOverlay)

		// Background picture over the whole screen, cropped instead of distorted.
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage)
				.Image(&ActiveStyle.Background)
			]
		]

		// Darker strip behind the menu.
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.WidthOverride(Style.ShadeWidth + Style.PanelOffsetX)
			[
				SNew(SImage)
				.Image(&ActiveStyle.Shade)
			]
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(Style.PanelOffsetX, 40.f, 40.f, 40.f))
		[
			SAssignNew(PanelHost, SBox)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return Config.Footer; })
			.Font(Style.SmallFont)
			.ColorAndOpacity(FSlateColor(Style.DimTextColor))
		];

	// Theme switch in the corner: names the theme a click leads to.
	if (Config.Themes.Num() > 1)
	{
		const int32 NextTheme = (ThemeIndex + 1) % Config.Themes.Num();
		Root->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(24.f))
		[
			SNew(SButton)
			.ButtonStyle(&ThemeButtonStyle)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(NSLOCTEXT("MainMenu", "ThemeTip", "Switch the menu look"))
			.OnClicked(FOnClicked::CreateUObject(this, &UMainMenuWidget::HandleThemeClicked))
			[
				SNew(STextBlock)
				.Text(Config.Themes[NextTheme].Name)
				.Font(Style.SmallFont)
				.ColorAndOpacity(FSlateColor(Style.TextColor))
			]
		];
	}

	PanelHost->SetContent(BuildPanel());
	return Root;
}

FReply UMainMenuWidget::HandleThemeClicked()
{
	if (Config.Themes.Num() > 1)
	{
		SetTheme((ThemeIndex + 1) % Config.Themes.Num());
	}
	return FReply::Handled();
}

void UMainMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PanelHost.Reset();
	RootHost.Reset();
}

TSharedRef<SWidget> UMainMenuWidget::BuildPanel()
{
	const FMainMenuStyle& Style = ActiveStyle;

	ButtonStyle = FButtonStyle()
		.SetNormal(Style.ButtonNormal)
		.SetHovered(Style.ButtonHovered)
		.SetPressed(Style.ButtonPressed)
		.SetDisabled(Style.ButtonDisabled)
		.SetNormalPadding(Style.ButtonPadding)
		.SetPressedPadding(Style.ButtonPadding + FMargin(0.f, 2.f, 0.f, -2.f));
	ApplySounds(ButtonStyle, Style.HoverSound, Style.ClickSound);

	AccentLineBrush = MakeColorBrush(Style.AccentColor * FLinearColor(1.f, 1.f, 1.f, 0.7f));

	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);

	if (!Config.Title.IsEmpty())
	{
		Column->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(Config.Title)
			.Font(Style.TitleFont)
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(FSlateColor(Style.TitleColor))
			.ShadowOffset(FVector2D(0.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		];
	}

	if (!Config.Subtitle.IsEmpty())
	{
		Column->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.f, 2.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(Config.Subtitle)
			.Font(Style.SubtitleFont)
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(FSlateColor(Style.AccentColor))
		];
	}

	// Thin accent line between title and buttons, as wide as the column.
	Column->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Fill)
	.Padding(FMargin(0.f, 18.f, 0.f, 22.f))
	[
		SNew(SBox)
		.HeightOverride(2.f)
		[
			SNew(SImage)
			.Image(&AccentLineBrush)
		]
	];

	// The buttons fill their box and the box is as wide as its widest child - so every button gets
	// the width of the longest label, and the box sits centred under the title.
	TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);

	for (int32 Index = 0; Index < Config.Entries.Num(); ++Index)
	{
		const FMainMenuEntry& Entry = Config.Entries[Index];
		const bool bUnlocked = IsEntryUnlocked(Index);
		const FText Hint = bUnlocked ? Entry.Description : (Entry.LockedHint.IsEmpty() ? Entry.Description : Entry.LockedHint);

		TSharedRef<SVerticalBox> Labels = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Entry.Label)
				.Font(Style.ButtonFont)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor(bUnlocked ? Style.TextColor : Style.LockedTextColor))
			];
		if (!Hint.IsEmpty())
		{
			Labels->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Hint)
				.Font(Style.SmallFont)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor(bUnlocked ? Style.DimTextColor : Style.LockedTextColor))
			];
		}

		Buttons->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(FMargin(0.f, (Index > 0 ? Style.ButtonSpacing : 0.f) + (Entry.bGapBefore ? Style.ButtonSpacing * 2.5f : 0.f), 0.f, 0.f))
		[
			SNew(SBox)
			.MinDesiredWidth(Style.ButtonSize.X)
			.MinDesiredHeight(Style.ButtonSize.Y)
			[
				SNew(SButton)
				.ButtonStyle(&ButtonStyle)
				.IsEnabled(bUnlocked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(FOnClicked::CreateUObject(this, &UMainMenuWidget::HandleEntryClicked, Index))
				[
					Labels
				]
			]
		];
	}

	Column->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	[
		Buttons
	];

	return SNew(SBorder)
		.BorderImage(&ActiveStyle.Panel)
		.Padding(Style.PanelPadding)
		[
			Column
		];
}

bool UMainMenuWidget::IsEntryUnlocked(int32 Index) const
{
	if (!Config.Entries.IsValidIndex(Index))
	{
		return false;
	}

	const FMainMenuEntry& Entry = Config.Entries[Index];
	if (Entry.RequiredSaveSlot.IsEmpty() && Entry.RequiredSwitchTag.IsNone())
	{
		return true;
	}

	if (!Entry.RequiredSaveSlot.IsEmpty() && UGameplayStatics::DoesSaveGameExist(Entry.RequiredSaveSlot, 0))
	{
		return true;
	}

	if (!Entry.RequiredSwitchTag.IsNone())
	{
		const UGameInstance* GI = GetGameInstance();
		if (const UMapSwitchSubsystem* MapSwitch = GI ? GI->GetSubsystem<UMapSwitchSubsystem>() : nullptr)
		{
			const FString MapName = Entry.RequiredSwitchTagMap.IsEmpty()
				? UGameplayStatics::GetCurrentLevelName(this, true)
				: Entry.RequiredSwitchTagMap;
			if (MapSwitch->IsSwitchEnabledForMap(MapName, Entry.RequiredSwitchTag))
			{
				return true;
			}
		}
	}

	return false;
}

FReply UMainMenuWidget::HandleEntryClicked(int32 Index)
{
	ActivateEntry(Index);
	return FReply::Handled();
}

void UMainMenuWidget::ActivateEntry(int32 Index)
{
	if (bTravelling || !IsEntryUnlocked(Index))
	{
		return;
	}

	const FMainMenuEntry& Entry = Config.Entries[Index];
	switch (Entry.Action)
	{
	case EMainMenuAction::OpenMap:
		OpenMap(Entry);
		break;

	case EMainMenuAction::ShowWidget:
		ShowPopup(Entry.WidgetClass.LoadSynchronous());
		break;

	case EMainMenuAction::SaveLoad:
		OpenSaveLoad();
		break;

	case EMainMenuAction::OpenURL:
		if (!Entry.URL.IsEmpty())
		{
			FPlatformProcess::LaunchURL(*Entry.URL, nullptr, nullptr);
		}
		break;

	case EMainMenuAction::Quit:
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	}
}

void UMainMenuWidget::OpenMap(const FMainMenuEntry& Entry)
{
	const FString MapName = Entry.TargetMap.ToSoftObjectPath().GetLongPackageName();
	if (MapName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainMenu] '%s' has no target map."), *Entry.Label.ToString());
		return;
	}

	bTravelling = true;

	// The RTS controller has the full travel path: tag, loading screen for every player, ServerTravel.
	if (ACameraControllerBase* RTSController = Cast<ACameraControllerBase>(GetOwningPlayer()))
	{
		RTSController->Server_TravelToMap(MapName, Entry.DestinationSwitchTagToEnable);
		return;
	}

	// Plain menu map: same steps by hand.
	if (!Entry.DestinationSwitchTagToEnable.IsNone())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMapSwitchSubsystem* MapSwitch = GI->GetSubsystem<UMapSwitchSubsystem>())
			{
				MapSwitch->MarkSwitchEnabledForMap(MapName, Entry.DestinationSwitchTagToEnable);
			}
			if (UGameSaveSubsystem* SaveSub = GI->GetSubsystem<UGameSaveSubsystem>())
			{
				SaveSub->PersistUnlocksToLatestSave();
			}
		}
	}

	// The map load blocks the game thread from the next frame on. Put the loading screen up and give it
	// a few frames to get drawn first, otherwise the frozen menu stays on screen for the whole load.
	UUserWidget* Loading = Config.LoadingWidgetClass ? CreateWidget<UUserWidget>(GetOwningPlayer(), Config.LoadingWidgetClass) : nullptr;
	if (Loading)
	{
		Loading->AddToViewport(PopupZOrder + 100);
	}

	UWorld* World = GetWorld();
	if (!Loading || !World || LoadingScreenLeadTime <= 0.f)
	{
		UGameplayStatics::OpenLevel(this, FName(*MapName));
		return;
	}

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, MapName]()
	{
		UGameplayStatics::OpenLevel(this, FName(*MapName));
	}), LoadingScreenLeadTime, false);
}

void UMainMenuWidget::ShowPopup(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return;
	}

	if (UUserWidget* Popup = CreateWidget<UUserWidget>(GetOwningPlayer(), WidgetClass))
	{
		Popup->AddToViewport(PopupZOrder);
	}
}

void UMainMenuWidget::OpenSaveLoad()
{
	if (!Config.SaveGameWidgetClass)
	{
		return;
	}

	if (USaveGameWidget* SaveWidget = CreateWidget<USaveGameWidget>(GetOwningPlayer(), Config.SaveGameWidgetClass))
	{
		// Without an owning actor the window closes itself (USaveGameWidget::CloseSelf).
		SaveWidget->InitializeWidget(Config.SaveSlotName, nullptr);
		SaveWidget->AddToViewport(PopupZOrder);
	}
}
