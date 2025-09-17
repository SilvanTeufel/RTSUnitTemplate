#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapSwitchWidget.generated.h"

class UTextBlock;
class UButton;
class AMapSwitchActor;

UCLASS()
class RTSUNITTEMPLATE_API UMapSwitchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeWidget(const FString& MapName, AMapSwitchActor* InOwningActor, bool Enabled);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DialogText;

    UPROPERTY(meta = (BindWidget))
    UButton* YesButton;

    UPROPERTY(meta = (BindWidget))
    UButton* NoButton;

    UPROPERTY(meta = (BindWidget))
    UButton* OkButton;

    UFUNCTION()
    void OnYesClicked();

    UFUNCTION()
    void OnNoClicked();

    UFUNCTION()
    void OnOkClicked();

private:
    FString TargetMapName;

    UPROPERTY()
    AMapSwitchActor* OwningActor;
};
