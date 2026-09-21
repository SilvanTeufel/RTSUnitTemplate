// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "GameplayTagContainer.h" // Include for FGameplayTag
#include "TaggedUnitSelector.generated.h"

/**
 * */
UCLASS()
class RTSUNITTEMPLATE_API UTaggedUnitSelector : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    ACustomControllerBase* ControllerBase;

    UFUNCTION()
    void InitWidget(ACustomControllerBase* InController);

    // --- Attributbaum und AbilityChooser direkt aus der linken Leiste (19.09.2026) ------------
    //
    // Beide Fenster hingen bisher nur an der Tab-Umschaltung und an der Einheitenauswahl. Ueber
    // diese zwei Funktionen laesst sich jedes von einem eigenen Knopf oeffnen UND schliessen,
    // ohne dass die Tab-Reihenfolge angefasst werden muss - dort bleiben sie unveraendert.

    /** Schaltet den Attributbaum ein und aus. */
    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void ToggleAttributeTree();

    /** Schaltet den AbilityChooser ein und aus. */
    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void ToggleAbilityChooser();

    // --- Pulsieren, solange Attributpunkte offen sind (19.09.2026) ---------------------------
    //
    // Der Attributbaum liegt hinter einem Knopf und einem Tab - dass ueberhaupt etwas zu
    // vergeben ist, sieht man sonst nie. Der Knopf atmet deshalb, solange irgendeine eigene
    // Einheit freie Punkte hat, und steht still, sobald alles vergeben ist.

    /** Dauer eines vollen Pulses in Sekunden. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Puls")
    float AttributePulseInterval = 1.2f;

    /** Wieviel groesser der Knopf im Scheitel wird (0.12 = 12 Prozent). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Puls")
    float AttributePulseScale = 0.12f;

    /**
     * Abstand zwischen zwei Pruefungen, ob noch Punkte offen sind.
     *
     * Die Pruefung laeuft ueber alle ALevelUnit der Karte - das gehoert NICHT in jeden Frame.
     * Der Puls selbst laeuft trotzdem weich, er rechnet nur mit dem letzten Ergebnis weiter.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Puls")
    float AttributePulseCheckInterval = 0.25f;

protected:

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Sind bei einer eigenen Einheit noch Attributpunkte offen? */
    bool HasSpendableAttributePoints() const;

private:
    float PulseTime = 0.f;
    float CheckTime = 0.f;
    bool bPointsSpendable = false;

protected:

    /**
     * BindWidgetOptional, nicht BindWidget: ein TaggedUnitSelector-Blueprint ohne diese beiden
     * Knoepfe muss weiter kompilieren. Bei BindWidget waere jedes bestehende Blueprint sofort
     * fehlerhaft, und davon gibt es mehrere im Projekt.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* AttributeTreeToggleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* AbilityChooserToggleButton;

    UFUNCTION()
    void HandleAttributeTreeToggleClicked();

    UFUNCTION()
    void HandleAbilityChooserToggleClicked();

    /** Liefert das Kamera-Pawn des eigenen Spielers, an dem beide Fenster haengen. */
    class AExtendedCameraBase* GetOwningCamera() const;

    // A single function to handle clicks from ANY of our TaggedUnitButton widgets.
    UFUNCTION()
    void HandleTaggedUnitButtonClicked(FGameplayTag UnitTag);

protected:
    // We now only need pointers to our custom UTaggedUnitButton widgets.
    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl1;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl2;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl3;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl4;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl5;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl6;
    
    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt1;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt2;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt3;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt4;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt5;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt6;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlQ;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlW;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlE;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlR;
};