// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "GameTimerWidget.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UGameTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* GameTime;

	/**
	 * Abschusszaehler des Endlos-Survival.
	 *
	 * BindWidgetOptional, NICHT BindWidget: jedes vorhandene HUD-Blueprint erbt von dieser
	 * Klasse und hat den Textblock nicht. Ein Pflicht-Bind wuerde sie alle beim naechsten
	 * Kompilieren zerreissen - fuer eine Anzeige, die nur eine Karte braucht.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* KillCount;

	/** Vorspann der Anzeige. Leer lassen, wenn das Blueprint selbst eine Beschriftung setzt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FText KillCountPrefix = FText::FromString(TEXT("Kills: "));

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Letzter angezeigter Wert - spart das Setzen des Textes in jedem Frame. */
	int32 ZuletztAngezeigt = -1;
};
