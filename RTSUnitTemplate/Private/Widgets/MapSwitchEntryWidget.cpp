// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MapSwitchEntryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Misc/Paths.h"

void UMapSwitchEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SelectButton)
    {
        SelectButton->OnClicked.AddDynamic(this, &UMapSwitchEntryWidget::OnSelectClicked);
    }
}

void UMapSwitchEntryWidget::SetEntry(const FMapSwitchDestinationState& InState, AMapSwitchActor* InOwningActor)
{
    State = InState;
    OwningActor = InOwningActor;

    const FString Caption = State.DisplayName.IsEmpty()
        ? FPaths::GetBaseFilename(State.MapLongPackageName)
        : State.DisplayName.ToString();

    if (EntryText)
    {
        EntryText->SetText(FText::FromString(Caption));
        EntryText->SetColorAndOpacity(FSlateColor(State.bUnlocked ? UnlockedTint : LockedTint));
    }

    if (SelectButton)
    {
        // Both: the button stops taking clicks AND it looks different. SetIsEnabled(false) alone
        // tints nothing in some styles, and a locked row then reads as an open one that simply
        // refuses to respond.
        SelectButton->SetIsEnabled(State.bUnlocked);
        SelectButton->SetBackgroundColor(State.bUnlocked ? UnlockedTint : LockedTint);
    }
}

void UMapSwitchEntryWidget::OnSelectClicked()
{
    if (OwningActor)
    {
        OwningActor->TravelToDestination(State);
    }
}
