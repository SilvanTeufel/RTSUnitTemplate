// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Widgets/MainMenuWidget.h"
#include "MainMenuActor.generated.h"

/**
 * Shows the widget main menu (UMainMenuWidget) when the map starts: put one in a menu map and fill
 * in Config. Switches the local player to UI input with a visible mouse cursor.
 */
UCLASS(meta = (DisplayName = "Main Menu Actor"))
class RTSUNITTEMPLATE_API AMainMenuActor : public AActor
{
	GENERATED_BODY()

public:
	AMainMenuActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	FMainMenuConfig Config;

	/** Subclass to use instead of UMainMenuWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	TSubclassOf<UMainMenuWidget> MenuWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu")
	int32 ZOrder = 10;

	UFUNCTION(BlueprintPure, Category = "MainMenu")
	UMainMenuWidget* GetMenuWidget() const { return MenuWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MenuWidget;
};
