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

    CreateClassUIElements();

    // Assuming you have a separate setup function for buttons
    //SetupButtons();

    // Bind Reset button click event
    if (ResetButton)
    {
        ResetButton->OnClicked.AddDynamic(this, &UTalentChooser::ResetClass);
    }

    SetVisibility(ESlateVisibility::Hidden);
}

void UTalentChooser::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update logic for the dynamic elements (if necessary)
    // ...
}

void UTalentChooser::CreateClassUIElements()
{
    UE_LOG(LogTemp, Warning, TEXT("Creating Class UI Elements"));
    // Clear existing elements
    ClassProgressBars.Empty();
    ClassButtons.Empty();
    ClassNames.Empty();

    // Check if OwnerUnitBase is valid and has talent data
    if (OwnerUnitBase.IsValid() && OwnerUnitBase->TalentDataArray.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("OwnerUnitBase is valid and has talent data"));
        for (const FClassTalentData& TalentData : OwnerUnitBase->TalentDataArray)
        {
            // Assuming FClassTalentData has a FClassTalentPoints field
            FClassTalentPoints TalentPoints = TalentData.TalentPoints;

            UE_LOG(LogTemp, Warning, TEXT("Processing TalentData for ClassId: %d"), TalentPoints.ClassId);
            // Array of properties to create UI elements for
            TArray<int32> Properties = {
                TalentPoints.ClassId,
                TalentPoints.RangeOffset,
                TalentPoints.WalkSpeedOffset,
                TalentPoints.AttackSpeedScaler,
                TalentPoints.PiercedTargetsOffset,
                TalentPoints.ProjectileScaler,
                TalentPoints.ProjectileSpeedScaler,
                TalentPoints.ProjectileCount,
                TalentPoints.HealthReg,
                TalentPoints.ShieldReg,
                TalentPoints.Ultimate
            };

            // Names for each property, used for text blocks
            TArray<FString> PropertyNames = {
                TEXT("Class ID"), 
                TEXT("Range Offset"), 
                TEXT("Walk Speed Offset"), 
                TEXT("Attack Speed Scaler"), 
                TEXT("Pierced Targets Offset"), 
                TEXT("Projectile Scaler"), 
                TEXT("Projectile Speed Scaler"), 
                TEXT("Projectile Count"), 
                TEXT("Health Regeneration"), 
                TEXT("Shield Regeneration"), 
                TEXT("Ultimate")
            };

            // Assuming 'this' is a UUserWidget or derived from UUserWidget
            if (UVerticalBox* VBox = Cast<UVerticalBox>(GetWidgetFromName(TEXT("VerticalBox"))))
            {
                for (int32 i = 0; i < Properties.Num(); ++i)
                {
                    UProgressBar* NewProgressBar = NewObject<UProgressBar>(VBox);
                    if (NewProgressBar)
                    {
                        NewProgressBar->SetPercent(0.5f); // Example setting
                        VBox->AddChildToVerticalBox(NewProgressBar);
                    }

                    UTalentButton* NewButton = NewObject<UTalentButton>(VBox);
                    if (NewButton)
                    {
                        NewButton->SetupButton(i);
                        NewButton->OnTalentButtonClicked.AddDynamic(this, &UTalentChooser::HandleTalentButtonClicked);
                        VBox->AddChildToVerticalBox(NewButton);
                    }

                    UTextBlock* NewTextBlock = NewObject<UTextBlock>(VBox);
                    if (NewTextBlock)
                    {
                        NewTextBlock->SetText(FText::FromString(PropertyNames[i]));
                        VBox->AddChildToVerticalBox(NewTextBlock);
                    }

                    UE_LOG(LogTemp, Warning, TEXT("Created UI elements for property: %s"), *PropertyNames[i]);
                }
            }
        }
    }
}

/*
 
 void UTalentChooser::CreateClassUIElements()
 {
     // Clear existing elements
     ClassProgressBars.Empty();
     ClassButtons.Empty();
     ClassNames.Empty();
 
     // Check if OwnerUnitBase is valid and has talent data
     if (OwnerUnitBase.IsValid() && OwnerUnitBase->TalentDataArray.Num() > 0)
     {
         for (int32 Index = 0; Index < OwnerUnitBase->TalentDataArray.Num(); ++Index)
         {
             // Assuming FClassTalentData has a FClassTalentPoints field
             FClassTalentPoints TalentPoints = OwnerUnitBase->TalentDataArray[Index].TalentPoints;
 
             // Fetch and initialize ProgressBar
             FString ProgressBarName = FString::Printf(TEXT("ProgressBar_%d"), Index);
             UProgressBar* ProgressBar = Cast<UProgressBar>(GetWidgetFromName(FName(*ProgressBarName)));
             if (ProgressBar)
             {
                 // Initialize ProgressBar here, for example:
                 ProgressBar->SetPercent( percentage based on TalentPoints );
                 ClassProgressBars.Add(ProgressBar);
             }
 
             // Fetch and initialize Button
             FString ButtonName = FString::Printf(TEXT("Button_%d"), Index);
             UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*ButtonName)));
             if (Button)
             {
                 // Initialize Button here
                 ClassButtons.Add(Button);
             }
 
             // Fetch and initialize TextBlock
             FString TextBlockName = FString::Printf(TEXT("TextBlock_%d"), Index);
             UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(*TextBlockName)));
             if (TextBlock)
             {
                 // Set TextBlock text here, for example:
                 TextBlock->SetText(FText::FromString( appropriate string from PropertyNames ));
                 ClassNames.Add(TextBlock);
             }
         }
     }
 }

 */

// Handler function for the button's click event
void UTalentChooser::HandleTalentButtonClicked(int32 ButtonIndex)
{
    if (OwnerUnitBase.IsValid())
    {
        // Assuming each button corresponds to a specific talent point in order
        for (FClassTalentData& TalentData : OwnerUnitBase->TalentDataArray)
        {
            switch (ButtonIndex)
            {
                    case 0: TalentData.TalentPoints.ClassId++; break;
                    case 1: TalentData.TalentPoints.RangeOffset++; break;
                    case 2: TalentData.TalentPoints.WalkSpeedOffset++; break;
                    case 3: TalentData.TalentPoints.AttackSpeedScaler++; break;
                    case 4: TalentData.TalentPoints.PiercedTargetsOffset++; break;
                    case 5: TalentData.TalentPoints.ProjectileScaler++; break;
                    case 6: TalentData.TalentPoints.ProjectileSpeedScaler++; break;
                    case 7: TalentData.TalentPoints.ProjectileCount++; break;
                    case 8: TalentData.TalentPoints.HealthReg++; break;
                    case 9: TalentData.TalentPoints.ShieldReg++; break;
                    case 10: TalentData.TalentPoints.Ultimate++; break;
                    default: break;

                // Optionally, update the UI to reflect the changes
                // ...
            }
        }
    }
}


void UTalentChooser::ResetClass()
{
    if (!OwnerUnitBase.IsValid())
        return;

    // Reset logic for the class
    // ...
}
