#include "System/StoryTriggerQueueSubsystem.h"
#include "Widgets/StoryWidgetBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Actors/StoryTriggerActor.h"
#include "Components/StoryTriggerComponent.h"
#include "Sound/SoundClass.h"

UStoryTriggerQueueSubsystem::UStoryTriggerQueueSubsystem()
{
}

void UStoryTriggerQueueSubsystem::Deinitialize()
{
	ClearActive();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextStoryTimerHandle);
	}
	Pending.Empty();
	Super::Deinitialize();
}

void UStoryTriggerQueueSubsystem::SetDefaultSoundVolume(float Volume)
{
	DefaultSoundVolume = Volume;
}

void UStoryTriggerQueueSubsystem::SetMasterVolume(float Volume)
{
	if (MasterSoundClass)
	{
		MasterSoundClass->Properties.Volume = Volume;
	}
}

float UStoryTriggerQueueSubsystem::GetMasterVolume() const
{
	if (MasterSoundClass)
	{
		return MasterSoundClass->Properties.Volume;
	}
	return 1.0f;
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

	if (bIsStoryActive)
	{
		bIsStoryActive = false;
		GlobalSoundMultiplier = 1.0f;

		if (CurrentItem.TriggeringSource.IsValid())
		{
			if (AStoryTriggerActor* Actor = Cast<AStoryTriggerActor>(CurrentItem.TriggeringSource.Get()))
			{
				Actor->OnStoryFinished.Broadcast();
			}
			else if (UStoryTriggerComponent* Comp = Cast<UStoryTriggerComponent>(CurrentItem.TriggeringSource.Get()))
			{
				Comp->OnStoryFinished.Broadcast();
			}
		}
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(NextStoryTimerHandle))
	{
		return; // Waiting for delay
	}

	if (Pending.Num() == 0)
	{
		return;
	}

	CurrentItem = Pending[0];
	Pending.RemoveAt(0);

	FStoryQueueItem Item = CurrentItem;

	// Fallback: if no widget class provided, default to base class
	if (!Item.WidgetClass)
	{
		Item.WidgetClass = UStoryWidgetBase::StaticClass();
	}

	bIsStoryActive = true;
	if (Item.TriggeringSource.IsValid())
	{
		if (AStoryTriggerActor* Actor = Cast<AStoryTriggerActor>(Item.TriggeringSource.Get()))
		{
			GlobalSoundMultiplier = Actor->LowerVolume;
		}
		else if (UStoryTriggerComponent* Comp = Cast<UStoryTriggerComponent>(Item.TriggeringSource.Get()))
		{
			GlobalSoundMultiplier = Comp->LowerVolume;
		}
	}
	else
	{
		GlobalSoundMultiplier = 0.4f; // Default lowering
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

	// Resolve soft references (prefer soft over hard)
	UTexture2D* ResolvedTexture = Item.Image;
	UMaterialInterface* ResolvedMaterial = Item.Material;
	if (Item.MaterialSoft.IsValid() || Item.MaterialSoft.ToSoftObjectPath().IsValid())
	{
		ResolvedMaterial = Item.MaterialSoft.LoadSynchronous();
	}
	if (!ResolvedMaterial && (Item.ImageSoft.IsValid() || Item.ImageSoft.ToSoftObjectPath().IsValid()))
	{
		ResolvedTexture = Item.ImageSoft.LoadSynchronous();
	}

	int32 SizeX = 0, SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	const FVector2D CenterPos(0.5f * SizeX + Item.OffsetX, 0.5f * SizeY + Item.OffsetY);

	Widget->AddToViewport();
	Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	Widget->SetPositionInViewport(CenterPos, false);
	Widget->StartStory(Item.Text, ResolvedTexture, ResolvedMaterial);

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
	ClearActive();

	// Add 3s delay before proceeding to next item
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NextStoryTimerHandle, this, &UStoryTriggerQueueSubsystem::TryPlayNext, 3.0f, false);
	}
}
