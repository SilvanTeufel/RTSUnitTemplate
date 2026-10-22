// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "TutorialReplayButton.generated.h"

/**
 * Button that starts the tutorial of the current map again (ATutorialActor::ReplayTutorial).
 * Drop it into any HUD - it needs no Blueprint logic and hides itself on maps without a tutorial.
 */
UCLASS(meta = (DisplayName = "Tutorial Replay Button"))
class RTSUNITTEMPLATE_API UTutorialReplayButton : public UButton
{
	GENERATED_BODY()

public:
	/** Hide the button on maps without an ATutorialActor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bHideWithoutTutorial = true;

	virtual void SynchronizeProperties() override;

private:
	UFUNCTION()
	void HandleReplayClicked();
};

/** Button that opens the Steam store page of the game (wishlist). Needs no Blueprint logic. */
UCLASS(meta = (DisplayName = "Wishlist Button"))
class RTSUNITTEMPLATE_API UWishlistButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wishlist")
	FString StoreURL = TEXT("https://store.steampowered.com/app/5053670/Electric_Noosphere/");

	virtual void SynchronizeProperties() override;

private:
	UFUNCTION()
	void HandleWishlistClicked();
};
