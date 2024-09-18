#include "Widgets/TalentChooser.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>
#include <Components/Button.h>
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "GameFramework/GameSession.h"

void UTalentChooser::NativeConstruct()
{
    Super::NativeConstruct();
    
    InitializeLevelAndTalentUI();
    
    CreateClassUIElements();

    SetVisibility(ESlateVisibility::Hidden);
}
/*
void UTalentChooser::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    UpdateProgressBars();
    UpdateExperience();
    UpdateLevelAndTalents();
}
*/
void UTalentChooser::UpdateWidget()
{
    UpdateProgressBars();
    UpdateExperience();
    UpdateLevelAndTalents();
}

void UTalentChooser::StartUpdateTimer()
{
    // Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
    GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UTalentChooser::UpdateWidget, UpdateInterval, true);
}

void UTalentChooser::StopTimer()
{
    // Check if the timer is currently active before attempting to clear it
    if (GetWorld()->GetTimerManager().IsTimerActive(UpdateTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
    }
}

void UTalentChooser::UpdateProgressBars()
{
    // Check if OwnerUnitBase and its Attributes are valid
    if (!OwnerUnitBase || !OwnerUnitBase->Attributes)
    {
        return;
    }

    for (int32 Index = 0; Index < Attributes.Num() && Index < ClassProgressBars.Num(); ++Index)
    {
        if (ClassProgressBars[Index])
        {
            float CurrentValue = Attributes[Index]->GetCurrentValue();
            float MaxValue = OwnerUnitBase->LevelUpData.MaxTalentsPerStat; // Replace with actual max value if available
            
            ClassProgressBars[Index]->SetPercent(CurrentValue / MaxValue);
        }

    }
}


void UTalentChooser::InitializeAttributes()
{
    if (OwnerUnitBase && OwnerUnitBase->Attributes)
    {
        // Clear the existing array
        Attributes.Empty();

        // Add attributes to the array
        Attributes.Add(&OwnerUnitBase->Attributes->Stamina);
        Attributes.Add(&OwnerUnitBase->Attributes->AttackPower);
        Attributes.Add(&OwnerUnitBase->Attributes->Willpower);
        Attributes.Add(&OwnerUnitBase->Attributes->Haste);
        Attributes.Add(&OwnerUnitBase->Attributes->Armor);
        Attributes.Add(&OwnerUnitBase->Attributes->MagicResistance);
    }
}

void UTalentChooser::UpdateExperience()
{
    if(!OwnerUnitBase || !OwnerUnitBase->Attributes) return;
    
    if(ExperienceProgressBar)
    {
        // Ensure Experience and CharacterLevel are not causing division by zero
        if (OwnerUnitBase->LevelData.Experience && OwnerUnitBase->LevelData.CharacterLevel)
        {
            float N = (float)(OwnerUnitBase->LevelUpData.ExperiencePerLevel * OwnerUnitBase->LevelData.CharacterLevel);
            float Z = (float)OwnerUnitBase->LevelData.Experience;
            //if(N > 0 && Z > 0 && Z < N)
                ExperienceProgressBar->SetPercent(Z / N);
        }
    }
}

void UTalentChooser::UpdateLevelAndTalents()
{
    if(!OwnerUnitBase || !OwnerUnitBase->Attributes) return;
        

    CurrentLevel->SetText(FText::AsNumber(OwnerUnitBase->LevelData.CharacterLevel));
    AvailableTalents->SetText(FText::AsNumber(OwnerUnitBase->LevelData.TalentPoints));
}

void UTalentChooser::CreateClassUIElements()
{

    if (!OwnerUnitBase || !OwnerUnitBase->Attributes)
    {
        // Handle the invalid case, perhaps by logging an error or setting up default values
        return;
    }
    
    InitializeLevelAndTalentUI();
    InitializeAttributes();
    // Clear existing elements
    ClassProgressBars.Empty();
    ClassButtons.Empty();
    ClassNames.Empty();

    for (int32 Index = 0; Index < Attributes.Num(); ++Index)
    {
        // Fetch and initialize ProgressBar
        FString ProgressBarName = FString::Printf(TEXT("ProgressBar_%d"), Index);
        UProgressBar* ProgressBar = Cast<UProgressBar>(GetWidgetFromName(FName(*ProgressBarName)));
        if (ProgressBar)
        {
            ProgressBar->SetPercent(Attributes[Index]->GetCurrentValue() / OwnerUnitBase->LevelUpData.MaxTalentsPerStat); // Attributes[Index]->GetBaseValue()
            ClassProgressBars.Add(ProgressBar);
        }

        // Fetch and initialize Button
        FString ButtonName = FString::Printf(TEXT("Button_%d"), Index);
        UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*ButtonName)));
 
        if (Button)
        {
            // You may want to bind a handler to each button
            //Button->SetupButton(Index);
            // Bind to the button's OnTalentButtonClicked delegate
            //Button->OnTalentButtonClicked.AddDynamic(this, &UTalentChooser::HandleTalentButtonClicked);

            ClassButtons.Add(Button);
        }

        // Fetch and initialize TextBlock
        FString TextBlockName = FString::Printf(TEXT("TextBlock_%d"), Index);
        UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(*TextBlockName)));
        if (TextBlock)
        {
            TextBlock->SetText(FText::FromString(AttributeNames[Index]));
            ClassNames.Add(TextBlock);
        }
    }
}



void UTalentChooser::InitializeLevelAndTalentUI()
{
    if(!OwnerUnitBase) return;
    

    CurrentLevel->SetText(FText::AsNumber(OwnerUnitBase->LevelData.CharacterLevel));
    AvailableTalents->SetText(FText::AsNumber(OwnerUnitBase->LevelData.TalentPoints));
    

}


// Handler function for the button's click event
void UTalentChooser::HandleTalentButtonClicked(int32 ButtonIndex)
{
    
    UE_LOG(LogTemp, Warning, TEXT("HandleTalentButtonClicked!"));

}



void UTalentChooser::OnResetTalentsClicked()
{
    if (OwnerUnitBase)
    {
        // Implement logic to reset talents here
        // For example, resetting the talent points and updating the UI
        OwnerUnitBase->ResetTalents();
        // Update UI if necessary
        if (AvailableTalents)
        {
            int32 TalentPoints = OwnerUnitBase->LevelData.TalentPoints; // Assuming TalentPoints is a property of ALevelUnit
            AvailableTalents->SetText(FText::AsNumber(TalentPoints));
        }
    }
}

void UTalentChooser::OnLevelUpClicked()
{
    if (OwnerUnitBase)
    {
        OwnerUnitBase->LevelUp();
        
        // Update UI to reflect new level and available talent points
        CurrentLevel->SetText(FText::AsNumber(OwnerUnitBase->LevelData.CharacterLevel));
        AvailableTalents->SetText(FText::AsNumber(OwnerUnitBase->LevelData.TalentPoints));

        // Optionally, refresh other parts of the UI
    }
}