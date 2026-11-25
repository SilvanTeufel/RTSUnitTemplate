// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MapMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"

void UMapMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Vor dem Slate-Aufbau einfuegen, damit die Reihenfolge in der Liste stimmt.
	UButton* KeyLayoutButton = Cast<UButton>(GetWidgetFromName(KeyLayoutButtonName));
	if (!KeyLayoutButton && WidgetTree)
	{
		UButton* Template = Cast<UButton>(GetWidgetFromName(KeyLayoutTemplateButtonName));
		if (!Template)
		{
			Template = ExitButton;
		}
		UPanelWidget* Parent = Template ? Template->GetParent() : nullptr;
		if (Parent)
		{
			KeyLayoutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), KeyLayoutButtonName);
			KeyLayoutButton->SetStyle(Template->GetStyle());
			KeyLayoutButton->SetColorAndOpacity(Template->GetColorAndOpacity());
			KeyLayoutButton->SetBackgroundColor(Template->GetBackgroundColor());

			// Die Beschriftung als Kopie der Vorlage: Schrift, Farbe, Ausrichtung und vor allem die
			// Mindestbreite - ueber sie sind alle Knoepfe im Menue gleich breit.
			UTextBlock* TemplateLabel = Template->GetChildrenCount() > 0 ? Cast<UTextBlock>(Template->GetChildAt(0)) : nullptr;
			UTextBlock* Label = TemplateLabel ? DuplicateObject<UTextBlock>(TemplateLabel, WidgetTree) : WidgetTree->ConstructWidget<UTextBlock>();
			Label->Slot = nullptr;
			Label->SetText(KeyLayoutButtonLabel);
			UButtonSlot* LabelSlot = Cast<UButtonSlot>(KeyLayoutButton->AddChild(Label));
			if (const UButtonSlot* TemplateLabelSlot = TemplateLabel ? Cast<UButtonSlot>(TemplateLabel->Slot) : nullptr)
			{
				if (LabelSlot)
				{
					LabelSlot->SetPadding(TemplateLabelSlot->GetPadding());
					LabelSlot->SetHorizontalAlignment(TemplateLabelSlot->GetHorizontalAlignment());
					LabelSlot->SetVerticalAlignment(TemplateLabelSlot->GetVerticalAlignment());
				}
			}

			UPanelSlot* NewSlot = Parent->InsertChildAt(Parent->GetChildIndex(Template) + 1, KeyLayoutButton);
			if (const UVerticalBoxSlot* From = Cast<UVerticalBoxSlot>(Template->Slot))
			{
				if (UVerticalBoxSlot* To = Cast<UVerticalBoxSlot>(NewSlot))
				{
					To->SetPadding(From->GetPadding());
					To->SetSize(From->GetSize());
					To->SetHorizontalAlignment(From->GetHorizontalAlignment());
					To->SetVerticalAlignment(From->GetVerticalAlignment());
				}
			}
			else if (const UHorizontalBoxSlot* FromH = Cast<UHorizontalBoxSlot>(Template->Slot))
			{
				if (UHorizontalBoxSlot* ToH = Cast<UHorizontalBoxSlot>(NewSlot))
				{
					ToH->SetPadding(FromH->GetPadding());
					ToH->SetSize(FromH->GetSize());
					ToH->SetHorizontalAlignment(FromH->GetHorizontalAlignment());
					ToH->SetVerticalAlignment(FromH->GetVerticalAlignment());
				}
			}
		}
	}
	if (KeyLayoutButton)
	{
		KeyLayoutButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnKeyLayoutClicked);
	}
}

void UMapMenuWidget::OnKeyLayoutClicked()
{
	if (AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetOwningPlayerPawn()))
	{
		Camera->OpenKeyLayoutChoice();
	}
}

void UMapMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// A switched-off map button stays visible but greyed out: disabled (no clicks, disabled draw
	// effect on the label) and dimmed, so it reads as "not available" with any button style.
	auto ApplyMapButtonEnabled = [](UButton* Button, bool bEnabled)
	{
		Button->SetIsEnabled(bEnabled);
		Button->SetRenderOpacity(bEnabled ? 1.f : 0.45f);
	};

	if (Map1Button)
	{
		Map1Button->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnMap1Clicked);
		ApplyMapButtonEnabled(Map1Button, bMap1Enabled);
	}

	if (Map2Button)
	{
		Map2Button->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnMap2Clicked);
		ApplyMapButtonEnabled(Map2Button, bMap2Enabled);
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

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddUniqueDynamic(this, &UMapMenuWidget::OnResumeClicked);
	}

	AktualisiereNachAufgabe();
}

bool UMapMenuWidget::CloseSubPage()
{
	UWidget* SubPage = SubPageWidgetName.IsNone() ? nullptr : GetWidgetFromName(SubPageWidgetName);
	UWidget* MainPage = MainPageWidgetName.IsNone() ? nullptr : GetWidgetFromName(MainPageWidgetName);
	if (!SubPage)
	{
		return false;
	}
	const bool bWasOpen = SubPage->GetVisibility() != ESlateVisibility::Collapsed && SubPage->GetVisibility() != ESlateVisibility::Hidden;
	SubPage->SetVisibility(ESlateVisibility::Collapsed);
	if (MainPage)
	{
		MainPage->SetVisibility(ESlateVisibility::Visible);
	}
	return bWasOpen;
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

void UMapMenuWidget::OnResumeClicked()
{
	// Zurueck ins Spiel heisst auch: eine Pause aus dem Menue endet hier.
	if (UWorld* World = GetWorld())
	{
		if (UGameplayStatics::IsGamePaused(World))
		{
			UGameplayStatics::SetGamePaused(World, false);
			AktualisierePauseBeschriftung();
		}
	}

	// Derselbe Weg wie Esc: Sichtbarkeit, BlockControls und Blur - siehe OnSurrenderClicked.
	if (AExtendedCameraBase* Kamera = Cast<AExtendedCameraBase>(GetOwningPlayerPawn()))
	{
		Kamera->CloseMapMenu();
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
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
	if (bAlreadyClicked || !bMap1Enabled) return;

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
	if (bAlreadyClicked || !bMap2Enabled) return;

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
