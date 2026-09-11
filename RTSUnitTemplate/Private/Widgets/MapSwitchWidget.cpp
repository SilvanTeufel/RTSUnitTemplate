// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MapSwitchWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Actors/MapSwitchActor.h"
#include "Misc/Paths.h"
#include "Controller/PlayerController/CameraControllerBase.h" // Include Player Controller
#include "System/MapSwitchSubsystem.h"
#include "Widgets/MapSwitchEntryWidget.h"
#include "Components/PanelWidget.h"

bool UMapSwitchWidget::SupportsDestinationList() const
{
    return DestinationList != nullptr && EntryWidgetClass != nullptr;
}

void UMapSwitchWidget::InitializeWidgetWithDestinations(const TArray<FMapSwitchDestinationState>& States, AMapSwitchActor* InOwningActor)
{
    OwningActor = InOwningActor;

    if (!SupportsDestinationList())
    {
        // Without a list nothing would be visible at all. Fall back to the first open entry.
        for (const FMapSwitchDestinationState& State : States)
        {
            if (State.bUnlocked)
            {
                InitializeWidget(State.MapLongPackageName, InOwningActor, true, State.DisplayName);
                return;
            }
        }
        InitializeWidget(FString(), InOwningActor, false, FText::GetEmpty());
        return;
    }

    if (DialogText)
    {
        DialogText->SetText(DestinationListTitle.IsEmpty()
            ? FText::FromString(TEXT("Where do you want to go?"))
            : DestinationListTitle);
    }

    // Yes/No belongs to the single-target dialog; in list mode each row's own button decides.
    if (YesButton) YesButton->SetVisibility(ESlateVisibility::Collapsed);
    if (OkButton)  OkButton->SetVisibility(ESlateVisibility::Collapsed);
    if (NoButton)  NoButton->SetVisibility(ESlateVisibility::Visible);

    DestinationList->ClearChildren();
    for (const FMapSwitchDestinationState& State : States)
    {
        if (UMapSwitchEntryWidget* Row = CreateWidget<UMapSwitchEntryWidget>(GetOwningPlayer(), EntryWidgetClass))
        {
            Row->SetEntry(State, InOwningActor);
            DestinationList->AddChild(Row);
        }
    }
}

void UMapSwitchWidget::InitializeWidget(const FString& MapName, AMapSwitchActor* InOwningActor, bool Enabled, const FText& DisplayName)
{
    TargetMapName = MapName;
    OwningActor = InOwningActor;

    if (Enabled)
    {
        if (DialogText)
        {
            // Use the actor's configured LevelDisplayName when set; otherwise fall back to the map file name.
            const FString MapDisplayName = DisplayName.IsEmpty() ? FPaths::GetBaseFilename(MapName) : DisplayName.ToString();
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
    // Vor dem Travel den Ziel-Switch-Tag für die Ziel-Map aktivieren
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UMapSwitchSubsystem* Subsystem = GI->GetSubsystem<UMapSwitchSubsystem>())
            {
                if (OwningActor && OwningActor->GetDestinationSwitchTagToEnable() != NAME_None && !TargetMapName.IsEmpty())
                {
                    Subsystem->MarkSwitchEnabledForMap(TargetMapName, OwningActor->GetDestinationSwitchTagToEnable());
                }
            }
        }
    }

    // Get the owning player controller
    ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
    if (PC)
    {
        if (OwningActor)
        {
            OwningActor->StartMapSwitch();
        }
        
        // Call the RPC on the player controller, which is owned by the client
        FName Tag = OwningActor ? OwningActor->GetDestinationSwitchTagToEnable() : NAME_None;
        PC->Server_TravelToMap(TargetMapName, Tag);
    }

    if (OwningActor)
    {
        OwningActor->CloseWidget();
    }
}

void UMapSwitchWidget::OnNoClicked()
{
    if (OwningActor)
    {
        OwningActor->CloseWidget();
    }
}

void UMapSwitchWidget::OnOkClicked()
{
    if (OwningActor)
    {
        OwningActor->CloseWidget();
    }
}
