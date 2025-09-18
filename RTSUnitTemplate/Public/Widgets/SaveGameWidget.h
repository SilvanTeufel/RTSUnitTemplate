#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
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

    UFUNCTION()
    void OnYesClicked();

    UFUNCTION()
    void OnNoClicked();

    UFUNCTION()
    void OnLoadClicked();

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

    UPROPERTY()
    ASaveGameActor* OwningActor;

    // Hält die pro-Button-Handler am Leben
    UPROPERTY()
    TArray<USaveSlotClickHandler*> SlotClickHandlers;
};
