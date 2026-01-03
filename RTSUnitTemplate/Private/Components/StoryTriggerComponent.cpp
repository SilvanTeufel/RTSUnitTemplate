#include "Components/StoryTriggerComponent.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Actors/StoryTriggerActor.h" // for FStoryTriggerRow
#include "Engine/World.h"

UStoryTriggerComponent::UStoryTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UStoryTriggerComponent::ShouldTrigger(float ChancePercent) const
{
	const float Clamped = FMath::Clamp(ChancePercent, 0.f, 100.f);
	if (Clamped <= 0.f)
	{
		return false;
	}
	if (Clamped >= 100.f)
	{
		return true;
	}
	const float Roll = FMath::FRandRange(0.f, 100.f);
	return Roll < Clamped;
}

bool UStoryTriggerComponent::BuildQueueItemFromRow(FName RowName, FStoryQueueItem& OutItem) const
{
	if (!StoryDataTable)
	{
		return false;
	}

	static const FString Context(TEXT("StoryTriggerComponent_Row"));
	if (const FStoryWidgetTable* Row = StoryDataTable->FindRow<FStoryWidgetTable>(RowName, Context, true))
	{
		OutItem.WidgetClass = Row->StoryWidgetClass;
		OutItem.Text = Row->StoryText;
		OutItem.Image = Row->StoryImage;
		OutItem.Material = Row->StoryMaterial;
		OutItem.ImageSoft = Row->StoryImageSoft;
		OutItem.MaterialSoft = Row->StoryMaterialSoft;
		OutItem.OffsetX = Row->ScreenOffsetX;
		OutItem.OffsetY = Row->ScreenOffsetY;
		OutItem.LifetimeSeconds = Row->WidgetLifetimeSeconds;
		OutItem.Sound = Row->TriggerSound;
		return true;
	}
	return false;
}

bool UStoryTriggerComponent::BuildQueueItemFromRandomRow(FStoryQueueItem& OutItem) const
{
	if (!StoryDataTable)
	{
		return false;
	}

	const TArray<FName> RowNames = StoryDataTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		return false;
	}

	const int32 Index = FMath::RandRange(0, RowNames.Num() - 1);
	return BuildQueueItemFromRow(RowNames[Index], OutItem);
}

void UStoryTriggerComponent::TryTriggerRandom(float ChancePercent)
{
	if (!ShouldTrigger(ChancePercent))
	{
		return;
	}

	FStoryQueueItem Item;
	if (!BuildQueueItemFromRandomRow(Item))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
			{
				Item.TriggeringSource = this;
				Queue->EnqueueStory(Item);
				OnStoryTriggered.Broadcast();
			}
		}
	}
}

void UStoryTriggerComponent::TriggerSpecific(FName RowName)
{
	FStoryQueueItem Item;
	if (!BuildQueueItemFromRow(RowName, Item))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
			{
				Item.TriggeringSource = this;
				Queue->EnqueueStory(Item);
				OnStoryTriggered.Broadcast();
			}
		}
	}
}
