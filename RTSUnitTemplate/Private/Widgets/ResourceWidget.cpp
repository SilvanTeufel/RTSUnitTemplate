#include "Widgets/ResourceWidget.h"
#include "GameModes/ResourceGameMode.h"
#include "Kismet/GameplayStatics.h"
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>

#include "Controller/ControllerBase.h"
#include "GameStates/ResourceGameState.h"

void UResourceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Initialize your widget's properties and bindings here
    UpdateTeamResourcesDisplay();
    UpdateWorkerCountDisplay();
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
        UpdateWorkerCountDisplay();
        // Reset the timer
        UpdateTimer = 0.0f;
    }
}

void UResourceWidget::UpdateTeamIdText()
{
    if (TeamIdText) // Always check if the pointer is valid
    {
        TeamIdText->SetText(FText::AsNumber(TeamId));
    }
}

void UResourceWidget::AddWorkerToResource(EResourceType ResourceType)
{
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GameMode)
    {
        GameMode->SetMaxWorkersForResourceType(TeamId, ResourceType, 1); // Assuming this function exists in GameMode
    }
}

void UResourceWidget::RemoveWorkerFromResource(EResourceType ResourceType)
{
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GameMode)
    {
        GameMode->SetMaxWorkersForResourceType(TeamId, ResourceType, -1); // Assuming this function exists in GameMode
    }
}

void UResourceWidget::UpdateTeamResourcesDisplay()
{
    
 // Cast to your specific game mode class
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameMode not found or is not of type AResourceGameMode."));
        return;
    }

    // Assuming your GameMode has a method like GetResourceAmount that takes a team ID and a resource type
    // and returns the amount of that resource for the team. You need to implement this method based on your setup.

    // Iterate over all resource types. This example assumes you have an enum class EResourceType
    // that lists all the resource types your game uses.
    for (EResourceType ResourceType : TEnumRange<EResourceType>())
    {
        float ResourceAmount = GameMode->GetResourceAttribute(TeamId, ResourceType);
        
        FString ResourceAmountText = FString::Printf(TEXT("%.1f"), ResourceAmount);

        // Update the UI element corresponding to the resource type
        switch (ResourceType)
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
            // No default case needed as we're iterating over the enum range
        }
    }
}

void UResourceWidget::UpdateWorkerCountDisplay()
{
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameMode not found or is not of type AResourceGameMode."));
        return;
    }

    // Assuming your GameMode has a method like GetWorkerCount that takes a team ID and a resource type
    // and returns the number of workers for that resource for the team.

    for (EResourceType ResourceType : TEnumRange<EResourceType>())
    {
        int32 WorkerCount = GameMode->GetMaxWorkersForResourceType(TeamId, ResourceType);

        FString WorkerCountText = FString::Printf(TEXT("%d"), WorkerCount);

        // Update the UI element corresponding to the worker count for each resource type
        switch (ResourceType)
        {
        case EResourceType::Primary:
            if (PrimaryWorkerCount) PrimaryWorkerCount->SetText(FText::FromString(WorkerCountText));
            break;
        case EResourceType::Secondary:
            if (SecondaryWorkerCount) SecondaryWorkerCount->SetText(FText::FromString(WorkerCountText));
            break;
        case EResourceType::Tertiary:
            if (TertiaryWorkerCount) TertiaryWorkerCount->SetText(FText::FromString(WorkerCountText));
            break;
        case EResourceType::Rare:
            if (RareWorkerCount) RareWorkerCount->SetText(FText::FromString(WorkerCountText));
            break;
        case EResourceType::Epic:
            if (EpicWorkerCount) EpicWorkerCount->SetText(FText::FromString(WorkerCountText));
            break;
        case EResourceType::Legendary:
            if (LegendaryWorkerCount) LegendaryWorkerCount->SetText(FText::FromString(WorkerCountText));
            break;
        default:
            // Consider logging or handling the default case if necessary
            break;
        }
    }
}
