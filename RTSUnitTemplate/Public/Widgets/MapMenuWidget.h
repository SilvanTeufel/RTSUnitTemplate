#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapMenuWidget.generated.h"

class UButton;

/**
 * MapMenuWidget for toggling map switches and exit game.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMapMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSoftObjectPtr<UWorld> Map1Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSoftObjectPtr<UWorld> Map2Name;

protected:
	UPROPERTY(meta = (BindWidget))
	UButton* Map1Button;

	UPROPERTY(meta = (BindWidget))
	UButton* Map2Button;

	UPROPERTY(meta = (BindWidget))
	UButton* ExitButton;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnMap1Clicked();

	UFUNCTION()
	void OnMap2Clicked();

	UFUNCTION()
	void OnExitClicked();

	bool bAlreadyClicked = false;
};
