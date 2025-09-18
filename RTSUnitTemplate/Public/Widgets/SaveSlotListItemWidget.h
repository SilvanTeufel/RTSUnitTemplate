#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveSlotListItemWidget.generated.h"

class UButton;
class UTextBlock;
class USaveGameWidget;

UCLASS()
class RTSUNITTEMPLATE_API USaveSlotListItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Initialisiert das Listenelement mit Slotname und Anzeigetext
    void InitializeItem(const FString& InSlotName, const FString& InDisplayText, USaveGameWidget* InOwner);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* SelectButton;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* LabelText;

    UFUNCTION()
    void OnSelectClicked();

private:
    FString SlotName;

    UPROPERTY()
    USaveGameWidget* OwnerWidget;
};
