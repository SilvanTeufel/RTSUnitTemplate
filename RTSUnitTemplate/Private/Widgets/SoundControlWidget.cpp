#include "Widgets/SoundControlWidget.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void USoundControlWidget::SetDefaultSoundVolume(float Volume)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
			{
				Queue->SetDefaultSoundVolume(Volume);
			}
		}
	}
}

float USoundControlWidget::GetDefaultSoundVolume() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
			{
				return Queue->GetDefaultSoundVolume();
			}
		}
	}
	return 1.0f;
}

void USoundControlWidget::SetMasterVolume(float Volume)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
			{
				Queue->SetMasterVolume(Volume);
			}
		}
	}
}

float USoundControlWidget::GetMasterVolume() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
			{
				return Queue->GetMasterVolume();
			}
		}
	}
	return 1.0f;
}
