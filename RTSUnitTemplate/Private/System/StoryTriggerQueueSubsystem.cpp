#include "System/StoryTriggerQueueSubsystem.h"
#include "Widgets/StoryWidgetBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

UStoryTriggerQueueSubsystem::UStoryTriggerQueueSubsystem()
{
}

void UStoryTriggerQueueSubsystem::Deinitialize()
{
	ClearActive();
	Pending.Empty();
 Super::Deinitialize();
}

void UStoryTriggerQueueSubsystem::EnqueueStory(const FStoryQueueItem& Item)
{
	Pending.Add(Item);
	TryPlayNext();
}

void UStoryTriggerQueueSubsystem::ClearActive()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActiveTimerHandle);
	}
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
}

void UStoryTriggerQueueSubsystem::TryPlayNext()
{
	if (ActiveWidget)
	{
		return; // Already showing something
	}
	if (Pending.Num() == 0)
	{
		return;
	}

	FStoryQueueItem Item = Pending[0];
	Pending.RemoveAt(0);

	if (!Item.WidgetClass)
	{
		// Skip invalid entries
		TryPlayNext();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	UStoryWidgetBase* Widget = CreateWidget<UStoryWidgetBase>(PC, Item.WidgetClass);
	if (!Widget)
	{
		TryPlayNext();
		return;
	}

	int32 SizeX = 0, SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	const FVector2D CenterPos(0.5f * SizeX + Item.OffsetX, 0.5f * SizeY + Item.OffsetY);

	Widget->AddToViewport();
	Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	Widget->SetPositionInViewport(CenterPos, false);
	Widget->StartStory(Item.Text, Item.Image);

	if (Item.Sound)
	{
		UGameplayStatics::PlaySound2D(World, Item.Sound);
	}

	ActiveWidget = Widget;

	if (Item.LifetimeSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(ActiveTimerHandle, this, &UStoryTriggerQueueSubsystem::OnActiveLifetimeFinished, Item.LifetimeSeconds, false);
	}
}

void UStoryTriggerQueueSubsystem::OnActiveLifetimeFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActiveTimerHandle);
	}
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
	// Proceed to next item
	TryPlayNext();
}
