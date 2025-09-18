#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Widgets/SaveGameWidget.h"
#include "SaveSlotClickHandler.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API USaveSlotClickHandler : public UObject
{
    GENERATED_BODY()
public:
    void Init(USaveGameWidget* InOwner, const FString& InSlotName)
    {
        Owner = InOwner;
        SlotName = InSlotName;
    }

    UFUNCTION()
    void HandleClick()
    {
        if (Owner)
        {
            Owner->HandleSlotItemClicked(SlotName);
        }
    }

private:
    UPROPERTY()
    USaveGameWidget* Owner = nullptr;

    UPROPERTY()
    FString SlotName;
};
