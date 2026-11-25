// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "WinLoseWidget.generated.h"

/**
 * "!!!! NEW HIGHSCORE !!!!" across the top of the screen, pulsing - shown by UWinLoseWidget when the survival run
 * just set a new best time on the map (USurvivalScoreSubsystem::WasNewHighscore). Builds its own layout.
 */
UCLASS()
class RTSUNITTEMPLATE_API UHighscoreBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(float Seconds, float PreviousBest, int32 Waves);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> PulseBox = nullptr;

	float Age = 0.f;
};

/**
 *
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWinLoseWidgetEvent, class UWinLoseWidget*);

UCLASS()
class RTSUNITTEMPLATE_API UWinLoseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The end screen opened / closed - the game adds its own buttons (e.g. the online leaderboard) to it. */
	static FOnWinLoseWidgetEvent OnShown;
	static FOnWinLoseWidgetEvent OnClosed;

	bool HasWon() const { return bWon; }

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ResultText;

	UPROPERTY(meta = (BindWidget))
	class UButton* OkButton;

	/** Optional: wechselt in den Zuschauermodus. Ausgegraut, wenn niemand mehr spielt. */
	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* SpectateButton;

	/**
	 * Optional: laedt dieselbe Karte neu.
	 *
	 * BindWidgetOptional, damit ein WinLose-Blueprint ohne diesen Knopf weiter kompiliert.
	 * Sichtbar nur nach einer Niederlage - nach einem Sieg waere ein Neustart sinnlos.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* RestartButton;

	void SetupWidget(bool bInWon, const FString& MapName, FName InDestinationSwitchTagToEnable);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(Transient)
	TObjectPtr<UHighscoreBannerWidget> HighscoreBanner = nullptr;

	UFUNCTION()
	void OnOkClicked();

	UFUNCTION()
	void OnSpectateClicked();

	UFUNCTION()
	void OnRestartClicked();

	bool bWon;
	FString TargetMapName;
	FName DestinationSwitchTagToEnable;
	bool bAlreadyClicked = false;
};
