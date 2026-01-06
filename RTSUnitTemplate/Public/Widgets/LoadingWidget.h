#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingWidget.generated.h"

/**
 * Loading Widget that shows up at the start of the game.
 */
UCLASS()
class RTSUNITTEMPLATE_API ULoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* LoadingBar;

	void SetupLoadingWidget(float InDuration);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float Duration = 0.f;
	float ElapsedTime = 0.f;
};
