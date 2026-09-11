// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * MapMenuWidget for toggling map switches and exit game.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMapMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSoftObjectPtr<UWorld> Map1Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSoftObjectPtr<UWorld> Map2Name;

protected:
	UPROPERTY(meta = (BindWidget))
	UButton* Map1Button;

	UPROPERTY(meta = (BindWidget))
	UButton* Map2Button;

	UPROPERTY(meta = (BindWidget))
	UButton* ExitButton;

	/**
	 * Startet die aktuelle Karte neu. Optional gebunden, damit vorhandene Menue-Widgets
	 * ohne diesen Knopf weiter kompilieren - wer ihn will, legt im Widget einen Button
	 * namens "RestartButton" an, mehr ist nicht noetig.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RestartButton;

	/**
	 * Haelt das Spiel an und laesst es wieder laufen. Wie RestartButton optional gebunden,
	 * damit Menue-Widgets ohne diesen Knopf weiter kompilieren.
	 *
	 * Das Widget selbst muss waehrend der Pause bedienbar bleiben - deshalb setzt der
	 * Konstruktor bIsFocusable und der Pausezustand wird ueber SetGamePaused geschaltet,
	 * das UMG nicht mit anhaelt.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* PauseButton;

	/** Beschriftung des Pauseknopfs, falls vorhanden - wechselt zwischen Pause und Weiter. */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* PauseLabel;

	/** Aufgeben. Loest die Niederlage fuer das eigene Team aus. */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SurrenderButton;

	/**
	 * Wiederholung ansehen und Zuschauen. Beide sind gesperrt, solange man noch mitspielt -
	 * sie ergeben erst Sinn, wenn man aufgegeben hat oder die Partie fuer einen vorbei ist.
	 * Die Klickbehandlung liegt weiterhin im Widget-Blueprint; hier wird nur freigeschaltet.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ReplayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SpectatorButton;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnMap1Clicked();

	UFUNCTION()
	void OnMap2Clicked();

	UFUNCTION()
	void OnExitClicked();

	UFUNCTION()
	void OnRestartClicked();

	UFUNCTION()
	void OnPauseClicked();

	UFUNCTION()
	void OnSurrenderClicked();

	/** Sperrt oder gibt Wiederholung und Zuschauen frei, je nachdem ob schon aufgegeben wurde. */
	void AktualisiereNachAufgabe();

	bool bHatAufgegeben = false;

	/** Schreibt die Beschriftung passend zum aktuellen Pausezustand. */
	void AktualisierePauseBeschriftung();

	bool bAlreadyClicked = false;
};
