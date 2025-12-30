#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "WinLoseWidget.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UWinLoseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ResultText;

	UPROPERTY(meta = (BindWidget))
	class UButton* OkButton;

	void SetupWidget(bool bInWon, const FString& MapName, FName InDestinationSwitchTagToEnable);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnOkClicked();

	bool bWon;
	FString TargetMapName;
	FName DestinationSwitchTagToEnable;
	bool bAlreadyClicked = false;
};
