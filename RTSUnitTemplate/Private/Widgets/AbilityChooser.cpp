
#include "Widgets/AbilityChooser.h"

#include "Controller/PlayerController/ControllerBase.h"

void UAbilityChooser::NativeConstruct()
{
    Super::NativeConstruct();
     UsedAbilityPointsTextArray.Empty();
    InitializeButtonArray(ButtonPreFixes[0], OffensiveAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[1], DefensiveAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[2], AttackAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[3], ThrowAbilityButtons);
    SetVisibility(ESlateVisibility::Hidden);
   // StartUpdateTimer();
}
/*
void UAbilityChooser::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateAbilityDisplay();
}*/

void UAbilityChooser::SetOwnerActor(AAbilityUnit* NewOwner)
{
    OwnerAbilityUnit = NewOwner;
}

void UAbilityChooser::StartUpdateTimer()
{
    // Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
    GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UAbilityChooser::UpdateAbilityDisplay, UpdateInterval, true);
}

void UAbilityChooser::StopTimer()
{
    // Check if the timer is currently active before attempting to clear it
    if (GetWorld()->GetTimerManager().IsTimerActive(UpdateTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
    }
}


void UAbilityChooser::UpdateAbilityDisplay()
{
    if (OwnerAbilityUnit)
    {
        if (OffensiveAbilityText)
        {
            OffensiveAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->OffensiveAbilityID))));
        }
        if (DefensiveAbilityText)
        {
            DefensiveAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->DefensiveAbilityID))));
        }
        if (AttackAbilityText)
        {
            AttackAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->AttackAbilityID))));
        }
        if (ThrowAbilityText)
        {
            ThrowAbilityText->SetText(FText::FromString(GetEnumValueAsString("EGASAbilityInputID", static_cast<int32>(OwnerAbilityUnit->ThrowAbilityID))));
        }
        
        if (AvailableAbilityPointsText)
        {
            AvailableAbilityPointsText->SetText(FText::AsNumber(OwnerAbilityUnit->LevelData.AbilityPoints));
        }

        if (UsedAbilityPointsText)
        {
            UsedAbilityPointsText->SetText(FText::AsNumber(OwnerAbilityUnit->LevelData.UsedAbilityPoints));
        }

        // Update UsedAbilityPointsTextArray
        for (int32 Index = 0; Index < UsedAbilityPointsTextArray.Num(); Index++)
        {
            if (UsedAbilityPointsTextArray[Index])
            {
                int32 UsedPoints = OwnerAbilityUnit->LevelData.UsedAbilityPointsArray.IsValidIndex(Index) ? OwnerAbilityUnit->LevelData.UsedAbilityPointsArray[Index] : 0;
                UsedAbilityPointsTextArray[Index]->SetText(FText::AsNumber(UsedPoints));
            }
        }
    }
}

FString UAbilityChooser::GetEnumValueAsString(const FString& EnumName, int32 EnumValue)
{
    UEnum* Enum = FindObject<UEnum>(nullptr, *EnumName, true);
    if (!Enum)
    {
        return FString("Invalid");
    }
    return Enum->GetNameByValue(EnumValue).ToString();
}
/*
FString UAbilityChooser::GetEnumValueAsString(const FString& EnumName, int32 EnumValue)
{
    UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
    if (!Enum)
    {
        return FString("Invalid");
    }
    return Enum->GetNameByValue(EnumValue).ToString();
}*/

void UAbilityChooser::InitializeButtonArray(const FString& ButtonPrefix, TArray<UButton*>& ButtonArray)
{
    ButtonArray.Empty();
    for (int32 Index = 0; Index < ButtonInitCount; ++Index) // Assuming you have 4 buttons per category
    {
        FString ButtonName = FString::Printf(TEXT("%s_%d"), *ButtonPrefix, Index);
        UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*ButtonName)));

            if (Button)
            {
                ButtonArray.Add(Button);
            }
     }
    
    FString TextBlockName = FString::Printf(TEXT("UsedAbilityPointArray_%s"), *ButtonPrefix); // Modify this based on your actual naming convention
    UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(*TextBlockName)));
    if (TextBlock)
    {
        UsedAbilityPointsTextArray.Add(TextBlock);
    }
}
