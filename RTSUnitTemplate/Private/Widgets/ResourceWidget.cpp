#include "Widgets/ResourceWidget.h"
#include "GameModes/ResourceGameMode.h"
#include "Kismet/GameplayStatics.h"
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>

#include "Controller/ControllerBase.h"

void UResourceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Initialize your widget's properties and bindings here
    UpdateTeamResourcesDisplay();

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PlayerController)
    {
        return;
    }
    
    // Cast to your custom player controller class, replace 'AMyPlayerController' with your actual class name
    AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
    if (!ControllerBase)
    {
        return;
    }

    // Get the SelectableTeamId from the player controller
    TeamId = ControllerBase->SelectableTeamId;
}

void UResourceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // You can update things every tick if needed
    UpdateTimer += InDeltaTime;

    // Check if the timer has reached the update interval
    if (UpdateTimer >= UpdateInterval)
    {
        
        // Update the team resources display
        UpdateTeamResourcesDisplay();

        // Reset the timer
        UpdateTimer = 0.0f;

        
    }
}

void UResourceWidget::UpdateTeamResourcesDisplay()
{
    // Get the current player controller

    if (TeamIdText) // Always check if the pointer is valid
    {
        TeamIdText->SetText(FText::AsNumber(TeamId));
     }
    
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!GameMode) return;


    const TArray<FResourceArray>& TeamResources = GameMode->TeamResources;

    for (const FResourceArray& ResourceArray : TeamResources)
    {
        if (ResourceArray.Resources.IsValidIndex(TeamId))
        {
            float ResourceAmount = ResourceArray.Resources[TeamId];
            FString ResourceAmountText = FString::Printf(TEXT("%f"), ResourceAmount);

            switch (ResourceArray.ResourceType)
            {
            case EResourceType::Primary:
                if (PrimaryResourceText) PrimaryResourceText->SetText(FText::FromString(ResourceAmountText));
                break;
            case EResourceType::Secondary:
                if (SecondaryResourceText) SecondaryResourceText->SetText(FText::FromString(ResourceAmountText));
                break;
            case EResourceType::Tertiary:
                if (TertiaryResourceText) TertiaryResourceText->SetText(FText::FromString(ResourceAmountText));
                break;
            case EResourceType::Rare:
                if (RareResourceText) RareResourceText->SetText(FText::FromString(ResourceAmountText));
                break;
            case EResourceType::Epic:
                if (EpicResourceText) EpicResourceText->SetText(FText::FromString(ResourceAmountText));
                break;
            case EResourceType::Legendary:
                if (LegendaryResourceText) LegendaryResourceText->SetText(FText::FromString(ResourceAmountText));
                break;
            default:
                // Handle any unexpected resource type
                break;
            }
        }
    }
}