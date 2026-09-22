// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
