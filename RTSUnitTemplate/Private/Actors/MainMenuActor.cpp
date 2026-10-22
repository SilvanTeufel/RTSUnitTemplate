// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/MainMenuActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMainMenuActor::AMainMenuActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	MenuWidgetClass = UMainMenuWidget::StaticClass();
}

void AMainMenuActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const TSubclassOf<UMainMenuWidget> ClassToUse = MenuWidgetClass ? MenuWidgetClass : TSubclassOf<UMainMenuWidget>(UMainMenuWidget::StaticClass());
	MenuWidget = CreateWidget<UMainMenuWidget>(PC, ClassToUse);
	if (!MenuWidget)
	{
		return;
	}

	MenuWidget->SetupMenu(Config);
	// UIOnly input focuses the widget - a plain UUserWidget refuses focus and logs an error.
	MenuWidget->SetIsFocusable(true);
	MenuWidget->AddToViewport(ZOrder);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}

void AMainMenuActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
