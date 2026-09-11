// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CheatWidget.generated.h"

/**
 * Kleines Entwickler-Panel fuer Testlaeufe.
 *
 * Ein schmaler Knopf klappt zwei Schalter auf: Ressourcen auffuellen und die aktuelle
 * Siegbedingung erfuellen. Damit laesst sich eine Kampagne durchklicken, ohne jedes Level
 * wirklich zu spielen - gedacht vor allem, um die Levelwechsel zu pruefen.
 *
 * Bewusst als UWidget mit eigener Slate-Oberflaeche gebaut und NICHT als UUserWidget mit
 * BindWidget: ein per Werkzeug eingefuegtes Widget ist standardmaessig keine Blueprint-Variable,
 * und dann bleibt jedes BindWidget-Feld still auf nullptr. Hier gibt es nichts zu verdrahten -
 * das Widget in den HUD legen genuegt.
 */
UCLASS()
class RTSUNITTEMPLATE_API UCheatWidget : public UWidget
{
	GENERATED_BODY()

public:
	UCheatWidget();

	/** Wieviel je Ressourcenart aufgefuellt wird. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	float ResourceFillAmount = 50000.f;

	/** Panel-Hintergrund. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat|Style")
	FLinearColor PanelColor = FLinearColor(0.010f, 0.015f, 0.027f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat|Style")
	FLinearColor AccentColor = FLinearColor(0.209f, 0.644f, 0.694f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat|Style", meta = (ClampMin = "6"))
	int32 FontSize = 11;

	/** Panel beim Start ausgeklappt zeigen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheat")
	bool bStartOpen = false;

	/** Fuer den Aufruf aus Blueprints, falls jemand die Schalter woanders haben will. */
	UFUNCTION(BlueprintCallable, Category = "Cheat")
	void FillResources();

	UFUNCTION(BlueprintCallable, Category = "Cheat")
	void WinCurrentLevel();

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FReply HandleToggleClicked();
	FReply HandleFillClicked();
	FReply HandleWinClicked();
	EVisibility GetPanelVisibility() const;
	FText GetToggleLabel() const;

	/** Der lokale Controller, ueber den beide Schalter laufen. */
	class AWidgetController* GetLocalWidgetController() const;

	bool bPanelOpen = false;
	TSharedPtr<class SWidget> MyRoot;
};
