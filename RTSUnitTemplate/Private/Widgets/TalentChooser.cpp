#include "Widgets/TalentChooser.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>
#include <Components/Button.h>
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Widgets/TalentButton.h"

void UTalentChooser::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize level and talent-related UI elements
    // Initialize attributes
    
    InitializeLevelAndTalentUI();
    
    CreateClassUIElements();

    SetVisibility(ESlateVisibility::Hidden);
}

void UTalentChooser::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    UpdateProgressBars();
    UpdateExperience();
    UpdateLevelAndTalents();
}


void UTalentChooser::UpdateProgressBars()
{
    // Check if OwnerUnitBase and its Attributes are valid
    if (!OwnerUnitBase.IsValid() || !OwnerUnitBase->Attributes)
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
    if (OwnerUnitBase.IsValid() && OwnerUnitBase->Attributes)
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
    if(!OwnerUnitBase.IsValid()) return;
    
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
    if(!OwnerUnitBase.IsValid()) return;
        

    CurrentLevel->SetText(FText::AsNumber(OwnerUnitBase->LevelData.CharacterLevel));
    AvailableTalents->SetText(FText::AsNumber(OwnerUnitBase->LevelData.TalentPoints));
}

void UTalentChooser::CreateClassUIElements()
{

    if (!OwnerUnitBase.IsValid() || !OwnerUnitBase->Attributes)
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
        UTalentButton* Button = Cast<UTalentButton>(GetWidgetFromName(FName(*ButtonName)));
 
        if (Button)
        {
            // You may want to bind a handler to each button
            Button->SetupButton(Index);
            // Bind to the button's OnTalentButtonClicked delegate
            Button->OnTalentButtonClicked.AddDynamic(this, &UTalentChooser::HandleTalentButtonClicked);

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
    if(!OwnerUnitBase.IsValid()) return;
    

    CurrentLevel->SetText(FText::AsNumber(OwnerUnitBase->LevelData.CharacterLevel));
    AvailableTalents->SetText(FText::AsNumber(OwnerUnitBase->LevelData.TalentPoints));
    
    // Bind LevelUp and ResetTalents buttons to their respective handlers
    if (LevelUpButton)
    {
        LevelUpButton->OnClicked.AddDynamic(this, &UTalentChooser::OnLevelUpClicked);
    }

    if (ResetTalentsButton)
    {
        ResetTalentsButton->OnClicked.AddDynamic(this, &UTalentChooser::OnResetTalentsClicked);
    }
}


// Handler function for the button's click event
void UTalentChooser::HandleTalentButtonClicked(int32 ButtonIndex)
{
    if (OwnerUnitBase.IsValid())
    {
        switch (ButtonIndex)
        {
        case 0: OwnerUnitBase->InvestPointIntoStamina(); break;
        case 1: OwnerUnitBase->InvestPointIntoAttackPower(); break;
        case 2: OwnerUnitBase->InvestPointIntoWillPower(); break;
        case 3: OwnerUnitBase->InvestPointIntoHaste(); break;
        case 4: OwnerUnitBase->InvestPointIntoArmor(); break;
        case 5: OwnerUnitBase->InvestPointIntoMagicResistance(); break;
            // Add cases for additional attributes
            default: break;
        }

        if (AvailableTalents)
        {
            int32 TalentPoints = OwnerUnitBase->LevelData.TalentPoints; // Assuming TalentPoints is a property of ALevelUnit
            AvailableTalents->SetText(FText::AsNumber(TalentPoints));
        }
        // Update UI here if needed
    }
}



void UTalentChooser::OnResetTalentsClicked()
{
    if (OwnerUnitBase.IsValid())
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
    if (OwnerUnitBase.IsValid())
    {
        OwnerUnitBase->LevelUp();
        
        // Update UI to reflect new level and available talent points
        CurrentLevel->SetText(FText::AsNumber(OwnerUnitBase->LevelData.CharacterLevel));
        AvailableTalents->SetText(FText::AsNumber(OwnerUnitBase->LevelData.TalentPoints));

        // Optionally, refresh other parts of the UI
    }
}