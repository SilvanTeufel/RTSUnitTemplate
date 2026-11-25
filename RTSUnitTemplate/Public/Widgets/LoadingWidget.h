// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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

	void SetupLoadingWidget(float InTotalDuration, float InServerWorldTimeStart);

	/** Gives LoadingBar the sci-fi look (dark glass, thin glowing frame, glowing fill) instead of the engine default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Loading")
	bool bStyleLoadingBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Loading", meta = (EditCondition = "bStyleLoadingBar"))
	FLinearColor BarFillColor = FLinearColor(0.10f, 0.78f, 0.95f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Loading", meta = (EditCondition = "bStyleLoadingBar"))
	FLinearColor BarBackgroundColor = FLinearColor(0.010f, 0.015f, 0.027f, 0.80f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Loading", meta = (EditCondition = "bStyleLoadingBar"))
	FLinearColor BarFrameColor = FLinearColor(0.209f, 0.644f, 0.694f, 1.f);

	/** Keeps the buttons on the loading screen clickable: they fire on mouse-down (a click survives a frame
	 *  hitch between press and release), and a mouse capture left behind by the game
	 *  viewport (e.g. a camera drag still held while the screen appeared) is released once no button is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Loading")
	bool bKeepButtonsResponsive = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float CaptureCheckTime = 0.f;
	float TotalDuration = 0.f;
	float ServerWorldTimeStart = 0.f;
};
