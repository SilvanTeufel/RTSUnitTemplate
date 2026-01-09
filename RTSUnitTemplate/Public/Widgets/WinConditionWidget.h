#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WinConditionWidget.generated.h"

/**
 * Widget that explains the current map's win condition to the player.
 */
UCLASS()
class RTSUNITTEMPLATE_API UWinConditionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ConditionText;

protected:
	virtual void NativeConstruct() override;

	void UpdateConditionText();
};
