
#include "Widgets/AbilityChooser.h"
#include "Components/Image.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Widgets/AbilityButton.h"

void UAbilityChooser::NativeConstruct()
{
    Super::NativeConstruct();
    UsedAbilityPointsTextArray.Empty();
    InitializeButtonArray(ButtonPreFixes[0], OffensiveAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[1], DefensiveAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[2], AttackAbilityButtons);
    InitializeButtonArray(ButtonPreFixes[3], ThrowAbilityButtons);
    InitializeAbilityIconArray("Ability", SelectAbilityIcons, SelectAbilityButtons);
    SetVisibility(ESlateVisibility::Hidden);

    ControllerBase = Cast<AExtendedControllerBase>(GetWorld()->GetFirstPlayerController());
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
        SetAbilityIcons();
        SetVisibleAbilityButtonCount();
        
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


void UAbilityChooser::ClearAbilityArray()
{
    if (!OwnerAbilityUnit || !ControllerBase)
    {
        return;
    }

    // Pick which array to clear in-place using a reference
    TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities =
        (ControllerBase->AbilityArrayIndex == 1) ? OwnerAbilityUnit->SecondAbilities :
        (ControllerBase->AbilityArrayIndex == 2) ? OwnerAbilityUnit->ThirdAbilities  :
        (ControllerBase->AbilityArrayIndex == 3) ? OwnerAbilityUnit->FourthAbilities :
                            OwnerAbilityUnit->DefaultAbilities;

    // Clear the selected array
    Abilities.Empty();
    
    // If you need to do anything else after clearing, do it here...
    UE_LOG(LogTemp, Log, TEXT("Cleared Ability array #%d"), ControllerBase->AbilityArrayIndex);
}




void UAbilityChooser::SetAbilityIcons()
{
    // If there are no units at all, bail early
    if (!OwnerAbilityUnit) return;

    
    // Figure out how many icons we can actually set
    const int32 Count = OwnerAbilityUnit->SelectableAbilities.Num();
    
    // Iterate and set each icon
    for (int32 i = 0; i < Count; i++)
    {
        UTexture2D* AbilityIcon = OwnerAbilityUnit->SelectableAbilities[i]->GetDefaultObject<UGameplayAbilityBase>()->AbilityIcon;
        // Safety checks: ensure the array slot in UnitIcons is valid,
        // and that the Unit has a valid Texture2D in UnitIcon
        if (SelectAbilityIcons[i])
        {
            // Set the brush of the UImage to the texture from your AUnitBase
            SelectAbilityIcons[i]->SetBrushFromTexture(AbilityIcon, true);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not set icon for unit index %d"), i);
        }
    }
}

void UAbilityChooser::SetVisibleAbilityButtonCount()
{

    if (!OwnerAbilityUnit) return;

    // Figure out how many icons we can actually set
    const int32 Count = OwnerAbilityUnit->SelectableAbilities.Num();

    const int32 MaxIndex = FMath::Min(SelectAbilityButtons.Num(), 
                                     SelectAbilityIcons.Num());

    //UE_LOG(LogTemp, Warning, TEXT("SelectAbilityButtons.Num() %d"), SelectAbilityButtons.Num());
    //UE_LOG(LogTemp, Warning, TEXT("SelectAbilityIcons.Num() %d"), SelectAbilityIcons.Num());
    //UE_LOG(LogTemp, Warning, TEXT("SelectableAbilityCount %d"), SelectableAbilityCount);
    for (int32 i = 0; i < MaxIndex; i++)
    {
        if (SelectAbilityButtons.IsValidIndex(i) && 
            SelectAbilityIcons.IsValidIndex(i))
        {
            if (i >= Count)
            {
                //UE_LOG(LogTemp, Warning, TEXT("Hide %d"), i);
                SelectAbilityButtons[i]->SetVisibility(ESlateVisibility::Hidden);
                SelectAbilityIcons[i]->SetVisibility(ESlateVisibility::Hidden);
            }
            else
            {
                //UE_LOG(LogTemp, Warning, TEXT("Visible %d"), i);
                SelectAbilityButtons[i]->SetVisibility(ESlateVisibility::Visible);
                SelectAbilityIcons[i]->SetVisibility(ESlateVisibility::Visible);
            }
        }
    }
}

void UAbilityChooser::InitializeAbilityIconArray(const FString& Prefix, TArray<UImage*>& IconArray, TArray<UAbilityButton*>& ButtonArray)
{

    ButtonArray.Empty();
    for (int32 Index = 0; Index < SelectableAbilityCount; ++Index) // Assuming you have 4 buttons per category
    {
        FString ButtonName = FString::Printf(TEXT("%s_%d"), *Prefix, Index);
        UAbilityButton* Button = Cast<UAbilityButton>(GetWidgetFromName(FName(*ButtonName)));
   
        if (Button)
        {
            Button->OnClicked.AddUniqueDynamic(Button, &UAbilityButton::OnClick);
            Button->Id = Index;
            ButtonArray.Add(Button);
        }
    }
    
    // Clear the array to start fresh
    IconArray.Empty();

    
    // Loop through however many icons you plan to fetch per category
    for (int32 Index = 0; Index < SelectableAbilityCount; ++Index)
    {
        // Construct the widget name based on a prefix + index
        FString IconName = FString::Printf(TEXT("%sIcon_%d"), *Prefix, Index);
        
        // Get the widget by name and cast it to UImage
        UImage* FoundIcon = Cast<UImage>(GetWidgetFromName(FName(*IconName)));
        if (FoundIcon)
        {
            IconArray.Add(FoundIcon);
        }
    }
}