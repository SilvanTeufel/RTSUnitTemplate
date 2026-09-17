// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Templates/SubclassOf.h"
#include "SaveGameWidget.generated.h"

class UTextBlock;
class UButton;
class UEditableTextBox;
class UScrollBox;
class ASaveGameActor;
class USaveSlotClickHandler;

UCLASS()
class RTSUNITTEMPLATE_API USaveGameWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeWidget(const FString& InSlotName, ASaveGameActor* InOwningActor);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DialogText;

    // Eingabefeld für Slotnamen
    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* SlotNameTextBox;

    // Liste aller vorhandenen Spielstände
    UPROPERTY(meta = (BindWidget))
    UScrollBox* SavedSlotsList;

    // Save (Ja)
    UPROPERTY(meta = (BindWidget))
    UButton* YesButton;

    // Cancel (Nein)
    UPROPERTY(meta = (BindWidget))
    UButton* NoButton;

    // Load
    UPROPERTY(meta = (BindWidget))
    UButton* LoadButton;

    /**
     * "New Game" - legt einen leeren Spielstand an. Loescht NICHTS.
     *
     * Getrennt vom Loeschknopf unten, und das ist der Punkt: ein neues Spiel zu beginnen ist
     * etwas anderes, als die bisherigen wegzuwerfen. Wer beides auf denselben Knopf legt,
     * verliert irgendwann Spielstaende, die er behalten wollte.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* NewGameButton;

    // ---- Alle Spielstaende loeschen --------------------------------------------------
    //
    // Alle fuenf sind BindWidgetOptional und nicht BindWidget: ein Widget-Blueprint, der
    // diesen Teil nicht baut, soll weiter laufen. Mit BindWidget wuerde er beim Kompilieren
    // scheitern, und das trifft auch fremde Projekte, die das Plugin benutzen.
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* ResetButton;

    /** Das Bestaetigungsfeld. Wird beim Aufbau ausgeblendet und erst auf Knopfdruck gezeigt. */
    UPROPERTY(meta = (BindWidgetOptional))
    UWidget* ResetConfirmPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* ResetConfirmYesButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* ResetConfirmNoButton;

    /** Text im Bestaetigungsfeld; nach dem Loeschen steht hier das Ergebnis. */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ResetConfirmText;

    /**
     * Wie lange der Knopf ohne Bestaetigungsfeld scharf bleibt (Sekunden).
     *
     * Nur fuer den Notnagel unten: ist kein ResetConfirmPanel gebaut, braucht es trotzdem zwei
     * Druecke. Ein Knopf, der beim ersten Klick alle Spielstaende loescht, darf nicht dadurch
     * entstehen, dass jemand vergessen hat das Popup anzulegen.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveGame|Reset")
    float ResetArmSeconds = 4.0f;

    // Optional: Custom classes for the generated list entry button/label
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveGame|List")
    TSubclassOf<UButton> SaveSlotButtonClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveGame|List")
    TSubclassOf<UTextBlock> SaveSlotLabelClass;

    UFUNCTION()
    void OnYesClicked();

    UFUNCTION()
    void OnNoClicked();

    UFUNCTION()
    void OnLoadClicked();

    /** Legt sofort einen leeren Spielstand an. Ohne Rueckfrage - es geht nichts verloren. */
    UFUNCTION()
    void OnNewGameClicked();

    /** Fragt nach - loescht nie selbst. */
    UFUNCTION()
    void OnResetClicked();

    /** Loescht wirklich. Nur von hier aus wird ResetAllProgress aufgerufen. */
    UFUNCTION()
    void OnResetConfirmYesClicked();

    UFUNCTION()
    void OnResetConfirmNoClicked();

    /** Blendet das Bestaetigungsfeld ein oder aus und entschaerft dabei den Notnagel. */
    void ShowResetConfirm(bool bShow);

    void PopulateSavesList();

public:
    // Wird vom Listeneintrag-Widget aufgerufen
    void HandleSlotItemClicked(const FString& InSlotName)
    {
        SlotName = InSlotName;
        if (SlotNameTextBox)
        {
            SlotNameTextBox->SetText(FText::FromString(SlotName));
        }
    }

private:
    FString SlotName;

    /** Zeitpunkt des ersten Druecks - nur fuer den Notnagel ohne Bestaetigungsfeld. */
    double ResetArmedAt = -1.0;

    UPROPERTY()
    ASaveGameActor* OwningActor;

    // Hält die pro-Button-Handler am Leben
    UPROPERTY()
    TArray<USaveSlotClickHandler*> SlotClickHandlers;
};
