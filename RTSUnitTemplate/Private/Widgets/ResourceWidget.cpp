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
        GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, 1); // Assuming this function exists in GameMode
    }
}

void UResourceWidget::RemoveWorkerFromResource(EResourceType ResourceType)
{
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GameMode)
    {
        GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, -1); // Assuming this function exists in GameMode
    }
}

void UResourceWidget::UpdateTeamResourcesDisplay()
{
    
    AResourceGameState* ResourceGameState = GetWorld()->GetGameState<AResourceGameState>();
    if (!ResourceGameState)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceGameState not found on client."));
        return;
    }
        

    TArray<FResourceArray>& TeamResources = ResourceGameState->TeamResources;
    
    if (TeamResources.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("TeamResources is empty or not initialized."));
        return; // Early exit if TeamResources is empty or not initialized
    }
    
    for (const FResourceArray& ResourceArray : TeamResources)
    {
        
        if (ResourceArray.Resources.IsValidIndex(TeamId))
        {
            float ResourceAmount = ResourceArray.Resources[TeamId];
            FString ResourceAmountText = FString::Printf(TEXT("%.1f"), ResourceAmount);
            
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

void UResourceWidget::UpdateWorkerCountDisplay()
{
    AResourceGameState* ResourceGameState = GetWorld()->GetGameState<AResourceGameState>();
    if (!ResourceGameState)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceGameState not found on client."));
        return;
    }
        

    TArray<FResourceArray>& TeamResources = ResourceGameState->TeamResources;
    
    if (TeamResources.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("TeamResources is empty or not initialized."));
        return; // Early exit if TeamResources is empty or not initialized
    }
    
    for (const FResourceArray& ResourceArray : TeamResources)
    {
        if (ResourceArray.MaxWorkers.IsValidIndex(TeamId))
        {
            int32 WorkerCount = ResourceArray.MaxWorkers[TeamId];

            FString WorkerCountText = FString::Printf(TEXT("%d"), WorkerCount);

            // Update the UI element corresponding to the worker count for each resource type
            switch (ResourceArray.ResourceType)
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
}


