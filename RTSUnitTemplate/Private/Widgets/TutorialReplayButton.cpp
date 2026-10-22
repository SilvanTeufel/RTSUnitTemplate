// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/TutorialReplayButton.h"

#include "Actors/TutorialActor.h"
#include "HAL/PlatformProcess.h"

void UTutorialReplayButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (IsDesignTime())
	{
		return;
	}
	OnClicked.AddUniqueDynamic(this, &UTutorialReplayButton::HandleReplayClicked);
	if (bHideWithoutTutorial && !ATutorialActor::HasTutorial(this))
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTutorialReplayButton::HandleReplayClicked()
{
	ATutorialActor::ReplayTutorial(this);
}

void UWishlistButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!IsDesignTime())
	{
		OnClicked.AddUniqueDynamic(this, &UWishlistButton::HandleWishlistClicked);
	}
}

void UWishlistButton::HandleWishlistClicked()
{
	if (!StoreURL.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*StoreURL, nullptr, nullptr);
	}
}
