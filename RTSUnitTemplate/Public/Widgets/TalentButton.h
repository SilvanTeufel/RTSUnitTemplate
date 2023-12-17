#pragma once

#include "Components/Button.h"
#include "TalentButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTalentButtonClickedDelegate, int32, ButtonId);

UCLASS()
class UTalentButton : public UButton
{
	GENERATED_BODY()

public:
	UTalentButton();

	// Delegate for button click event
	FTalentButtonClickedDelegate OnTalentButtonClicked;

	// Method to set up the button's properties
	void SetupButton(int32 InButtonId);

private:
	// The button's unique identifier
	int32 ButtonId;

	// Handler for the button click event
	UFUNCTION()
	void OnClick();
};