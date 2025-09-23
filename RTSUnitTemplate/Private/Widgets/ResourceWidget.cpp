// ResourceWidget.cpp
#include "Widgets/ResourceWidget.h"
#include "Widgets/ResourceEntryWidget.h" // Include our new widget
#include "GameStates/ResourceGameState.h"
#include "GameModes/ResourceGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h" // For UEnum

void UResourceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Population now happens after TeamId is set, to ensure we have the correct data.
}


void UResourceWidget::SetTeamId(int32 Id)
{
    TeamId = Id;
    UpdateTeamIdText();
    PopulateResourceList();
    UpdateWidget(); // Initial update
}

void UResourceWidget::UpdateTeamIdText()
{
    if (TeamIdText) // Always check if the pointer is valid
    {
        TeamIdText->SetText(FText::AsNumber(TeamId));
    }
}

void UResourceWidget::StartUpdateTimer()
{
    GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UResourceWidget::UpdateWidget, UpdateInterval, true);
}

void UResourceWidget::StopTimer()
{
    GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
}

void UResourceWidget::PopulateResourceList()
{
    // Basic validation
    if (!ResourceEntryWidgetClass || !ResourceEntriesBox)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceEntryWidgetClass or ResourceEntriesBox is not set in WBP_ResourceWidget."));
        return;
    }
    
    AResourceGameState* ResourceGameState = GetWorld()->GetGameState<AResourceGameState>();
    if (!ResourceGameState) return;

    // Clear any existing entries before adding new ones
    ResourceEntriesBox->ClearChildren();

    // Get the Enum object to convert enum values to strings
    const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EResourceType"), true);
    if (!EnumPtr) return;

    // Loop through the game state's resource data to create a widget for each type
    for (const FResourceArray& ResourceArray : ResourceGameState->TeamResources)
    {
        UResourceEntryWidget* EntryWidget = CreateWidget<UResourceEntryWidget>(this, ResourceEntryWidgetClass);
        if (EntryWidget)
        {
            float ResourceAmount = ResourceArray.Resources.IsValidIndex(TeamId) ? ResourceArray.Resources[TeamId] : 0.0f;
            int32 WorkerCount = ResourceArray.MaxWorkers.IsValidIndex(TeamId) ? ResourceArray.MaxWorkers[TeamId] : 0;
            FText ResourceName = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(ResourceArray.ResourceType));

            // Set the data on the new widget
            EntryWidget->SetResourceData(ResourceArray.ResourceType, ResourceName, ResourceAmount, WorkerCount, TeamId);

            // Add the newly created widget to our vertical box
            //ResourceEntriesBox->AddChildToVerticalBox(EntryWidget);
            ResourceEntriesBox->AddChild(EntryWidget);
        }
    }
}

void UResourceWidget::UpdateWidget()
{
    AResourceGameState* ResourceGameState = GetWorld()->GetGameState<AResourceGameState>();
    if (!ResourceGameState || !ResourceEntriesBox || ResourceEntriesBox->GetChildrenCount() == 0)
    {
        return;
    }

    const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EResourceType"), true);
    if (!EnumPtr)
    {
        return;
    }

    // Create a Map for fast data lookups.
    TMap<EResourceType, const FResourceArray*> ResourceDataMap;
    for (const FResourceArray& ResourceData : ResourceGameState->TeamResources)
    {
        ResourceDataMap.Add(ResourceData.ResourceType, &ResourceData);
    }

    // Loop through UI widgets and update them using the map.
    for (UWidget* ChildWidget : ResourceEntriesBox->GetAllChildren())
    {
        if (UResourceEntryWidget* Entry = Cast<UResourceEntryWidget>(ChildWidget))
        {
            // Find data in the map instantly.
            if (const FResourceArray** FoundData = ResourceDataMap.Find(Entry->ResourceType))
            {
                const FResourceArray* ResourceArray = *FoundData;
                const float ResourceAmount = ResourceArray->Resources.IsValidIndex(TeamId) ? ResourceArray->Resources[TeamId] : 0.0f;
                const int32 WorkerCount = ResourceArray->MaxWorkers.IsValidIndex(TeamId) ? ResourceArray->MaxWorkers[TeamId] : 0;
                const FText ResourceNameText = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(ResourceArray->ResourceType));
                
                Entry->SetResourceData(ResourceArray->ResourceType, ResourceNameText, ResourceAmount, WorkerCount, TeamId);
            }
        }
    }
}