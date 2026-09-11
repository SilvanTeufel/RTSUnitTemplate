// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MapMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Characters/Camera/ExtendedCameraBase.h"

void UMapMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Map1Button)
	{
		Map1Button->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnMap1Clicked);
	}

	if (Map2Button)
	{
		Map2Button->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnMap2Clicked);
	}

	if (ExitButton)
	{
		ExitButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnExitClicked);
	}

	if (RestartButton)
	{
		RestartButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnRestartClicked);
	}

	if (PauseButton)
	{
		PauseButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnPauseClicked);
		AktualisierePauseBeschriftung();
	}

	if (SurrenderButton)
	{
		SurrenderButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnSurrenderClicked);
	}

	AktualisiereNachAufgabe();
}

void UMapMenuWidget::AktualisiereNachAufgabe()
{
	// Wiederholung und Zuschauen sind erst sinnvoll, wenn man nicht mehr mitspielt.
	if (ReplayButton)    ReplayButton->SetIsEnabled(bHatAufgegeben);
	if (SpectatorButton) SpectatorButton->SetIsEnabled(bHatAufgegeben);
	if (SurrenderButton) SurrenderButton->SetIsEnabled(!bHatAufgegeben);
}

void UMapMenuWidget::OnSurrenderClicked()
{
	if (bHatAufgegeben) return;

	if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer()))
	{
		PC->Server_Surrender();
		bHatAufgegeben = true;
		AktualisiereNachAufgabe();

		// Das Menue vollstaendig schliessen. Nur die Sichtbarkeit zu setzen reichte NICHT:
		// der Blur blieb stehen und BlockControls blieb true, der Spieler sah also ein
		// verschwommenes Bild und kam nicht ans Spiel. CloseMapMenu macht alle drei Schritte,
		// genau wie der Esc-Weg.
		if (AExtendedCameraBase* Kamera = Cast<AExtendedCameraBase>(GetOwningPlayerPawn()))
		{
			Kamera->CloseMapMenu();
		}
		else
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMapMenuWidget::AktualisierePauseBeschriftung()
{
	if (!PauseLabel) return;
	const bool bPausiert = UGameplayStatics::IsGamePaused(GetWorld());
	PauseLabel->SetText(bPausiert ? NSLOCTEXT("RTSUnitTemplate", "MenueWeiter", "Weiter")
	                              : NSLOCTEXT("RTSUnitTemplate", "MenuePause", "Pause"));
}

void UMapMenuWidget::OnPauseClicked()
{
	// Bewusst OHNE die bAlreadyClicked-Sperre: die gilt fuer Aktionen, die das Menue
	// verlassen (Kartenwechsel, Neustart, Beenden). Pause soll beliebig oft umschaltbar sein.
	//
	// SetGamePaused haelt nur die Spielwelt an; das Menue bleibt bedienbar, weil UMG in der
	// Pause weiterlaeuft. Im Netzwerkspiel greift es nur auf dem Server - ein Client bekommt
	// ein false zurueck und die Beschriftung bleibt dann korrekt auf "Pause" stehen.
	UWorld* World = GetWorld();
	if (!World) return;

	const bool bPausiert = UGameplayStatics::IsGamePaused(World);
	UGameplayStatics::SetGamePaused(World, !bPausiert);
	AktualisierePauseBeschriftung();
}

void UMapMenuWidget::OnRestartClicked()
{
	if (bAlreadyClicked) return;

	bAlreadyClicked = true;
	if (Map1Button)   Map1Button->SetIsEnabled(false);
	if (Map2Button)   Map2Button->SetIsEnabled(false);
	if (ExitButton)   ExitButton->SetIsEnabled(false);
	if (RestartButton) RestartButton->SetIsEnabled(false);

	// Der Controller kennt die aktuelle Karte und reist ueber Server_TravelToMap -
	// damit klappt es auch, wenn ein Client den Knopf drueckt.
	if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer()))
	{
		PC->RequestRestartCurrentMap();
	}
}

void UMapMenuWidget::OnMap1Clicked()
{
	if (bAlreadyClicked) return;

	FString MapToTravel = Map1Name.IsNull() ? "" : Map1Name.ToSoftObjectPath().GetLongPackageName();
	if (MapToTravel.IsEmpty()) return;

	bAlreadyClicked = true;

	if (Map1Button) Map1Button->SetIsEnabled(false);
	if (Map2Button) Map2Button->SetIsEnabled(false);
	if (ExitButton) ExitButton->SetIsEnabled(false);

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (PC)
	{
		PC->Server_TravelToMap(MapToTravel, NAME_None);
	}
}

void UMapMenuWidget::OnMap2Clicked()
{
	if (bAlreadyClicked) return;

	FString MapToTravel = Map2Name.IsNull() ? "" : Map2Name.ToSoftObjectPath().GetLongPackageName();
	if (MapToTravel.IsEmpty()) return;

	bAlreadyClicked = true;

	if (Map1Button) Map1Button->SetIsEnabled(false);
	if (Map2Button) Map2Button->SetIsEnabled(false);
	if (ExitButton) ExitButton->SetIsEnabled(false);

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (PC)
	{
		PC->Server_TravelToMap(MapToTravel, NAME_None);
	}
}

void UMapMenuWidget::OnExitClicked()
{
	if (bAlreadyClicked) return;
	bAlreadyClicked = true;

	if (Map1Button) Map1Button->SetIsEnabled(false);
	if (Map2Button) Map2Button->SetIsEnabled(false);
	if (ExitButton) ExitButton->SetIsEnabled(false);

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}
