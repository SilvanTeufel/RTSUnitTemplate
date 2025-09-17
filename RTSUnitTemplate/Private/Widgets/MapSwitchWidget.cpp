﻿#include "Widgets/MapSwitchWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Actors/MapSwitchActor.h"
#include "Misc/Paths.h"
#include "Controller/PlayerController/CameraControllerBase.h" // Include Player Controller

void UMapSwitchWidget::InitializeWidget(const FString& MapName, AMapSwitchActor* InOwningActor, bool Enabled)
{
    TargetMapName = MapName;
    OwningActor = InOwningActor;

    if (Enabled)
    {
        if (DialogText)
        {
            FString MapDisplayName = FPaths::GetBaseFilename(MapName);
            FString Question = FString::Printf(TEXT("Do you want to switch to map '%s'?"), *MapDisplayName);
            DialogText->SetText(FText::FromString(Question));
        }
        if (YesButton) YesButton->SetVisibility(ESlateVisibility::Visible);
        if (NoButton) NoButton->SetVisibility(ESlateVisibility::Visible);
        if (OkButton) OkButton->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        if (DialogText)
        {
            DialogText->SetText(FText::FromString(TEXT("You are not allowed to Travel there yet")));
        }
        if (YesButton) YesButton->SetVisibility(ESlateVisibility::Collapsed);
        if (NoButton) NoButton->SetVisibility(ESlateVisibility::Collapsed);
        if (OkButton) OkButton->SetVisibility(ESlateVisibility::Visible);
    }
}

void UMapSwitchWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (YesButton)
    {
        YesButton->OnClicked.AddDynamic(this, &UMapSwitchWidget::OnYesClicked);
    }

    if (NoButton)
    {
        NoButton->OnClicked.AddDynamic(this, &UMapSwitchWidget::OnNoClicked);
    }

    if (OkButton)
    {
        OkButton->OnClicked.AddDynamic(this, &UMapSwitchWidget::OnOkClicked);
    }
}

void UMapSwitchWidget::OnYesClicked()
{
    // Get the owning player controller
    ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
    if (PC)
    {
        // Call the RPC on the player controller, which is owned by the client
        PC->Server_TravelToMap(TargetMapName);
    }

    // The widget can be closed immediately.
    RemoveFromParent();
}

void UMapSwitchWidget::OnNoClicked()
{
    RemoveFromParent();
}

void UMapSwitchWidget::OnOkClicked()
{
    RemoveFromParent();
}
