// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Actors/MapSwitchActor.h"
#include "MapSwitchEntryWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * One row in the map-switch list: a button carrying a level name.
 *
 * A locked entry is deliberately shown rather than hidden - the player is supposed to see which
 * missions this planet still holds. The button is disabled and dimmed, so it reads as "later",
 * not as "missing".
 */
UCLASS()
class RTSUNITTEMPLATE_API UMapSwitchEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void SetEntry(const FMapSwitchDestinationState& InState, AMapSwitchActor* InOwningActor);

    /** Colour a locked row is tinted with. Exposed so a project can match its own palette. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FLinearColor LockedTint = FLinearColor(0.45f, 0.45f, 0.45f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FLinearColor UnlockedTint = FLinearColor::White;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* SelectButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* EntryText;

    UFUNCTION()
    void OnSelectClicked();

private:
    UPROPERTY()
    AMapSwitchActor* OwningActor = nullptr;

    FMapSwitchDestinationState State;
};
