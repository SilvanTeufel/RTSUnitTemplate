#include "Widgets/StoryWidgetBase.h"
#include "TimerManager.h"
#include "Engine/Texture2D.h"

void UStoryWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize UI state
	RevealedCount = 0;
	if (StoryText)
	{
		StoryText->SetText(FText::GetEmpty());
	}
	if (StoryImage)
	{
		StoryImage->SetBrushFromTexture(DefaultImage.Get());
	}

	if (bAutoStart && (!DefaultText.IsEmpty()))
	{
		StartStory(DefaultText, DefaultImage);
	}
}

void UStoryWidgetBase::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
	Super::NativeDestruct();
}

void UStoryWidgetBase::StartStory(const FText& InFullText, UTexture2D* InImage)
{
	if (StoryImage)
	{
		UTexture2D* UseTex = InImage ? InImage : DefaultImage.Get();
		StoryImage->SetBrushFromTexture(UseTex);
	}

	FullString = InFullText.ToString();
	RevealedCount = 0;
	if (StoryText)
	{
		StoryText->SetText(FText::FromString(TEXT("")));
	}

	if (UWorld* World = GetWorld())
	{
		const float Interval = 1.0f / FMath::Max(CharactersPerSecond, 1.0f);
		World->GetTimerManager().SetTimer(RevealTimerHandle, this, &UStoryWidgetBase::UpdateTextOnce, Interval, true);
	}
}

void UStoryWidgetBase::ResetStory()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
	FullString.Empty();
	RevealedCount = 0;
	if (StoryText)
	{
		StoryText->SetText(FText::GetEmpty());
	}
	if (StoryImage)
	{
		StoryImage->SetBrushFromTexture(nullptr);
	}
}

void UStoryWidgetBase::RevealAll()
{
	if (!FullString.IsEmpty())
	{
		RevealedCount = FullString.Len();
		if (StoryText)
		{
			StoryText->SetText(FText::FromString(FullString));
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
}

void UStoryWidgetBase::UpdateTextOnce()
{
	if (FullString.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevealTimerHandle);
		}
		return;
	}

	RevealedCount = FMath::Clamp(RevealedCount + 1, 0, FullString.Len());
	if (StoryText)
	{
		StoryText->SetText(FText::FromString(FullString.Left(RevealedCount)));
	}

	if (RevealedCount >= FullString.Len())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevealTimerHandle);
		}
	}
}
