
#include "Widgets/AbilityChooser.h"

#include "Controller/ControllerBase.h"

void UAbilityChooser::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateAbilityDisplay();
}

void UAbilityChooser::SetOwnerActor(AAbilityUnit* NewOwner)
{
    OwnerAbilityUnit = NewOwner;
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
            UE_LOG(LogTemp, Log, TEXT("UsedAbilityPointsTextArray.Num(): %d"), UsedAbilityPointsTextArray.Num());
            UE_LOG(LogTemp, Log, TEXT("Ability Index: %d"), Index);
            if (UsedAbilityPointsTextArray[Index])
            {
               
                int32 UsedPoints = OwnerAbilityUnit->LevelData.UsedAbilityPointsArray.IsValidIndex(Index) ? OwnerAbilityUnit->LevelData.UsedAbilityPointsArray[Index] : 0;
                UsedAbilityPointsTextArray[Index]->SetText(FText::AsNumber(UsedPoints));
                UE_LOG(LogTemp, Log, TEXT("Ability Index: %d, Used Points: %d"), Index, UsedPoints);
            }
        }
    }
}

FString UAbilityChooser::GetEnumValueAsString(const FString& EnumName, int32 EnumValue)
{
    UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
    if (!Enum)
    {
        return FString("Invalid");
    }
    return Enum->GetNameByValue(EnumValue).ToString();
}

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
