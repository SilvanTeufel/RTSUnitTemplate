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
	static const FString Context(TEXT("StoryTriggerComponent_Random"));
	if (const FStoryWidgetTable* Row = StoryDataTable->FindRow<FStoryWidgetTable>(RowNames[Index], Context, true))
	{
		OutItem.WidgetClass = Row->StoryWidgetClass;
		OutItem.Text = Row->StoryText;
		OutItem.Image = Row->StoryImage;
		OutItem.OffsetX = Row->ScreenOffsetX;
		OutItem.OffsetY = Row->ScreenOffsetY;
		OutItem.LifetimeSeconds = Row->WidgetLifetimeSeconds;
		OutItem.Sound = Row->TriggerSound;
		return true;
	}
	return false;
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
				Queue->EnqueueStory(Item);
			}
		}
	}
}
