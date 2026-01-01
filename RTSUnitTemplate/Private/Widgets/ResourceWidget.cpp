// ResourceWidget.cpp
#include "Widgets/ResourceWidget.h"
#include "Widgets/ResourceEntryWidget.h" // Include our new widget
#include "GameStates/ResourceGameState.h"
#include "GameModes/ResourceGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h" // For UEnum
#include "Engine/Texture2D.h"

UResourceWidget::UResourceWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set default collapse behavior for specific resources
    ResourceWorkerUICollapseOverrides.Add(EResourceType::Rare, true);
    ResourceWorkerUICollapseOverrides.Add(EResourceType::Epic, true);
    ResourceWorkerUICollapseOverrides.Add(EResourceType::Legendary, true);
}

void UResourceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Try an initial population so the widget shows something even before SetTeamId is called.
    // This will be safely overwritten when SetTeamId is invoked later.
    PopulateResourceList();
    UpdateWidget();
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
    const UEnum* EnumPtr = StaticEnum<EResourceType>();
    if (!EnumPtr) return;

    // Loop through the game state's resource data to create a widget for each type
    int32 AddedCount = 0;
    for (const FResourceArray& ResourceArray : ResourceGameState->TeamResources)
    {
        if (MaxResourcesToDisplay > 0 && AddedCount >= MaxResourcesToDisplay)
        {
            break;
        }

        UResourceEntryWidget* EntryWidget = CreateWidget<UResourceEntryWidget>(this, ResourceEntryWidgetClass);
        if (EntryWidget)
        {
            float ResourceAmount = ResourceArray.Resources.IsValidIndex(TeamId) ? ResourceArray.Resources[TeamId] : 0.0f;
            float MaxResourceAmount = ResourceArray.MaxResources.IsValidIndex(TeamId) ? ResourceArray.MaxResources[TeamId] : 0.0f;
            int32 WorkerCount = ResourceArray.MaxWorkers.IsValidIndex(TeamId) ? ResourceArray.MaxWorkers[TeamId] : 0;
            bool bIsSupplyLike = ResourceGameState->IsSupplyLike.IsValidIndex(static_cast<int32>(ResourceArray.ResourceType)) ? ResourceGameState->IsSupplyLike[static_cast<int32>(ResourceArray.ResourceType)] : false;

            // Name override or default from enum
            FText ResourceName = ResourceDisplayNames.Contains(ResourceArray.ResourceType)
                ? ResourceDisplayNames[ResourceArray.ResourceType]
                : EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(ResourceArray.ResourceType));

            // Icon override (may be null)
            UTexture2D* const* FoundIcon = ResourceIcons.Find(ResourceArray.ResourceType);
            UTexture2D* IconTexture = FoundIcon ? *FoundIcon : nullptr;

            // Get collapse override or default to false
            const bool* bShouldCollapseOverride = ResourceWorkerUICollapseOverrides.Find(ResourceArray.ResourceType);
            bool bCollapseWorkerUI = bShouldCollapseOverride ? *bShouldCollapseOverride : false;

            // Set the data on the new widget
            EntryWidget->SetResourceData(ResourceArray.ResourceType, ResourceName, ResourceAmount, WorkerCount, TeamId, IconTexture, MaxResourceAmount, bIsSupplyLike, bCollapseWorkerUI);

            // Add the newly created widget to our vertical box
            ResourceEntriesBox->AddChild(EntryWidget);
            ++AddedCount;
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

    const UEnum* EnumPtr = StaticEnum<EResourceType>();
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
                const float MaxResourceAmount = ResourceArray->MaxResources.IsValidIndex(TeamId) ? ResourceArray->MaxResources[TeamId] : 0.0f;
                const int32 WorkerCount = ResourceArray->MaxWorkers.IsValidIndex(TeamId) ? ResourceArray->MaxWorkers[TeamId] : 0;
                const bool bIsSupplyLike = ResourceGameState->IsSupplyLike.IsValidIndex(static_cast<int32>(ResourceArray->ResourceType)) ? ResourceGameState->IsSupplyLike[static_cast<int32>(ResourceArray->ResourceType)] : false;

                // Name override or default
                FText ResourceNameText = ResourceDisplayNames.Contains(ResourceArray->ResourceType)
                    ? ResourceDisplayNames[ResourceArray->ResourceType]
                    : EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(ResourceArray->ResourceType));

                // Icon override
                UTexture2D* const* FoundIcon = ResourceIcons.Find(ResourceArray->ResourceType);
                UTexture2D* IconTexture = FoundIcon ? *FoundIcon : nullptr;

                // Get collapse override or default to false
                const bool* bShouldCollapseOverride = ResourceWorkerUICollapseOverrides.Find(ResourceArray->ResourceType);
                bool bCollapseWorkerUI = bShouldCollapseOverride ? *bShouldCollapseOverride : false;
                
                Entry->SetResourceData(ResourceArray->ResourceType, ResourceNameText, ResourceAmount, WorkerCount, TeamId, IconTexture, MaxResourceAmount, bIsSupplyLike, bCollapseWorkerUI);
            }
        }
    }
}