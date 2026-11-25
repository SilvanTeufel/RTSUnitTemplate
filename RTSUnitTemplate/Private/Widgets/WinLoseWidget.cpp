// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/WinLoseWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Kismet/GameplayStatics.h"
#include "System/GameSaveSubsystem.h"
#include "System/MapSwitchSubsystem.h"
#include "Engine/GameInstance.h"
#include "System/SurvivalScoreSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UHighscoreBannerWidget::Setup(float Seconds, float PreviousBest, int32 Waves)
{
	if (!WidgetTree)
	{
		return;
	}
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>();
	Plate->SetBrushColor(FLinearColor(0.02f, 0.01f, 0.f, 0.78f));
	Plate->SetPadding(FMargin(48.f, 18.f));
	if (UOverlaySlot* S = Root->AddChildToOverlay(Plate))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(0.f, 90.f, 0.f, 0.f));
	}
	PulseBox = Plate;
	Plate->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Plate->SetContent(Column);
	auto MakeLine = [this, Column](const FText& Text, int32 Size, const FLinearColor& Colour, int32 Outline) -> UTextBlock*
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.OutlineSettings.OutlineSize = Outline;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		Block->SetFont(Font);
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Colour));
		Block->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Block))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		return Block;
	};
	TitleText = MakeLine(NSLOCTEXT("Survival", "NewHighscore", "!!!!  NEW HIGHSCORE  !!!!"), 64, FLinearColor(1.f, 0.78f, 0.1f, 1.f), 4);
	const FString Time = USurvivalScoreSubsystem::FormatSeconds(Seconds);
	const FText Detail = PreviousBest > 0.f
		? FText::Format(NSLOCTEXT("Survival", "HighscoreBeat", "You survived {0} (wave {1}) - your old best was {2}"),
			FText::FromString(Time), FText::AsNumber(Waves), FText::FromString(USurvivalScoreSubsystem::FormatSeconds(PreviousBest)))
		: FText::Format(NSLOCTEXT("Survival", "HighscoreFirst", "You survived {0} (wave {1}) - the best time on this map"),
			FText::FromString(Time), FText::AsNumber(Waves));
	MakeLine(Detail, 26, FLinearColor::White, 2);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHighscoreBannerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	if (PulseBox)
	{
		// Slams in large, then keeps pulsing.
		const float Intro = FMath::Clamp(Age / 0.35f, 0.f, 1.f);
		const float Scale = FMath::Lerp(2.4f, 1.f, FMath::InterpEaseOut(0.f, 1.f, Intro, 3.f)) + 0.06f * FMath::Sin(Age * 5.f) * Intro;
		PulseBox->SetRenderScale(FVector2D(Scale));
	}
	if (TitleText)
	{
		const float Blink = 0.5f + 0.5f * FMath::Sin(Age * 7.f);
		TitleText->SetColorAndOpacity(FSlateColor(FMath::Lerp(FLinearColor(1.f, 0.62f, 0.f, 1.f), FLinearColor(1.f, 1.f, 0.75f, 1.f), Blink)));
	}
}

void UWinLoseWidget::SetupWidget(bool bInWon, const FString& MapName, FName InDestinationSwitchTagToEnable)
{
	bWon = bInWon;
	TargetMapName = MapName;
	DestinationSwitchTagToEnable = InDestinationSwitchTagToEnable;
	if (ResultText)
	{
		ResultText->SetText(FText::FromString(bWon ? TEXT("You Won!") : TEXT("You Lost!")));
	}
}

void UWinLoseWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (OkButton)
	{
		OkButton->OnClicked.AddUniqueDynamic(this, &UWinLoseWidget::OnOkClicked);
	}

	APlayerController* PC = GetOwningPlayer();

	// Zuschauen nur, wenn noch ein anderer Spieler ohne Ergebnis in der Partie ist. Die Antwort
	// kommt vom Server (siehe ACameraControllerBase::bSpectateAvailable); ein Client kann sie
	// nicht selbst ermitteln, weil er die fremden PlayerController gar nicht kennt.
	if (SpectateButton)
	{
		SpectateButton->OnClicked.AddUniqueDynamic(this, &UWinLoseWidget::OnSpectateClicked);

		const ACameraControllerBase* CamPC = Cast<ACameraControllerBase>(PC);
		SpectateButton->SetIsEnabled(CamPC != nullptr && CamPC->bSpectateAvailable);
	}

	// Neustart nur nach einer Niederlage anbieten.
	if (RestartButton)
	{
		RestartButton->OnClicked.AddUniqueDynamic(this, &UWinLoseWidget::OnRestartClicked);
		RestartButton->SetVisibility(bWon ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (PC && IsValid(PC))
	{
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeUIOnly());
	}

	// Endless survival with a new best time: say it LOUD, above everything else.
	float Seconds = 0.f, PreviousBest = 0.f;
	int32 Waves = 0;
	if (!HighscoreBanner && PC && USurvivalScoreSubsystem::WasNewHighscore(this, Seconds, PreviousBest, Waves))
	{
		HighscoreBanner = CreateWidget<UHighscoreBannerWidget>(PC, UHighscoreBannerWidget::StaticClass());
		if (HighscoreBanner)
		{
			HighscoreBanner->Setup(Seconds, PreviousBest, Waves);
			HighscoreBanner->AddToViewport(1000);
		}
	}

	OnShown.Broadcast(this);
}

FOnWinLoseWidgetEvent UWinLoseWidget::OnShown;
FOnWinLoseWidgetEvent UWinLoseWidget::OnClosed;

void UWinLoseWidget::NativeDestruct()
{
	OnClosed.Broadcast(this);
	if (HighscoreBanner)
	{
		HighscoreBanner->RemoveFromParent();
		HighscoreBanner = nullptr;
	}
	Super::NativeDestruct();
}

void UWinLoseWidget::OnRestartClicked()
{
	if (bAlreadyClicked) return;
	bAlreadyClicked = true;

	UWorld* World = GetWorld();
	if (!World) return;

	// Dieselbe Karte noch einmal laden. GetCurrentLevelName ohne Praefix liefert den Namen, den
	// OpenLevel erwartet; ein Reisen ueber den Server ist hier falsch, weil der Neustart den
	// gesamten Spielstand verwerfen soll.
	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString*/ true);
	UE_LOG(LogTemp, Warning, TEXT("[WinLose] Neustart der Karte '%s'."), *CurrentMap);

	RemoveFromParent();

	if (APlayerController* OwningPC = GetOwningPlayer())
	{
		OwningPC->SetShowMouseCursor(false);
		OwningPC->SetInputMode(FInputModeGameOnly());
	}

	UGameplayStatics::OpenLevel(World, FName(*CurrentMap));
}

void UWinLoseWidget::OnSpectateClicked()
{
	ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	// Server-Aufruf, damit der Knopf auch beim Client wirkt - der Pawn-Tausch liegt beim GameMode.
	PC->Server_EnterSpectate(/*bRevealAll=*/false);

	// Das Fenster muss weg, sonst schaut der Spieler durch FInputModeUIOnly auf ein totes Menue.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	RemoveFromParent();
}

void UWinLoseWidget::OnOkClicked()
{
	if (bAlreadyClicked) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* OwningPC = GetOwningPlayer();
	if (!OwningPC || !IsValid(OwningPC))
	{
		return;
	}

	bAlreadyClicked = true;

	if (OkButton)
	{
		OkButton->SetIsEnabled(false);
	}

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(OwningPC);
	const bool bWillTravel = (PC != nullptr) && !TargetMapName.IsEmpty();

	if (UGameInstance* GI = OwningPC->GetGameInstance())
	{
		// Only arm the quick-save when we will actually travel AND only on the authority. Otherwise the
		// PostLoadMapWithWorld binding would leak (no map load to consume it) and a client would write its
		// own incomplete save during the transition. (P3/P5)
		if (bWillTravel && OwningPC->HasAuthority())
		{
			if (UGameSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UGameSaveSubsystem>())
			{
				SaveSubsystem->SetPendingQuickSave(true);
			}
		}

		if (UMapSwitchSubsystem* MapSwitchSub = GI->GetSubsystem<UMapSwitchSubsystem>())
		{
			if (bWon && !TargetMapName.IsEmpty() && DestinationSwitchTagToEnable != NAME_None)
			{
				MapSwitchSub->MarkSwitchEnabledForMap(TargetMapName, DestinationSwitchTagToEnable);
			}
		}
	}

	if (bWillTravel)
	{
		PC->Server_TravelToMap(TargetMapName, DestinationSwitchTagToEnable);
		// The pending ServerTravel will tear down this world (and this widget); nothing more to do.
		return;
	}

	// No travel target (e.g. a lose screen with no destination map): don't leave the player trapped in
	// FInputModeUIOnly behind a dead, disabled OK button. Restore normal RTS input and dismiss the widget. (P6)
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	OwningPC->SetInputMode(InputMode);
	OwningPC->SetShowMouseCursor(true);
	RemoveFromParent();
}
