// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Actors/MapSwitchActor.h"
#include "MapSwitchWidget.generated.h"

class UTextBlock;
class UButton;
class UPanelWidget;
class UMapSwitchEntryWidget;
class AMapSwitchActor;

UCLASS()
class RTSUNITTEMPLATE_API UMapSwitchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeWidget(const FString& MapName, AMapSwitchActor* InOwningActor, bool Enabled, const FText& DisplayName = FText::GetEmpty());

    /**
     * True when this widget blueprint actually has the pieces for a list (a panel named
     * DestinationList and an EntryWidgetClass). Widgets authored before this existed answer false,
     * and the actor then falls back to the single-target Yes/No dialog.
     */
    UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
    bool SupportsDestinationList() const;

    /** Fills the list with one row per destination; locked rows come out disabled and dimmed. */
    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void InitializeWidgetWithDestinations(const TArray<FMapSwitchDestinationState>& States, AMapSwitchActor* InOwningActor);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DialogText;

    /** Container the level rows are added to. Optional - without it the widget is single-target. */
    UPROPERTY(meta = (BindWidgetOptional))
    UPanelWidget* DestinationList;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UMapSwitchEntryWidget> EntryWidgetClass;

    /** Heading shown above the list. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FText DestinationListTitle;

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
