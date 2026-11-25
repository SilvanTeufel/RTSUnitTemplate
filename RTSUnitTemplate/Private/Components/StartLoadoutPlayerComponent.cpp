// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Components/StartLoadoutPlayerComponent.h"
#include "GameModes/StartLoadoutComponent.h"
#include "GameModes/RTSGameModeBase.h"
#include "Widgets/StartLoadoutWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Widgets/ControlWidget.h"
#include "Widgets/KeyLayoutWidgets.h"
#include "Controller/Input/RTSKeyBindings.h"
#include "HAL/IConsoleManager.h"

bool UStartLoadoutPlayerComponent::OpenKeyLayoutChoiceFirst(UMaterialInterface* CardMaterial, TFunction<void()> Then)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	const URTSKeyBindingSubsystem* Bindings = URTSKeyBindingSubsystem::Get(this);
	if (!bAskKeyLayoutFirst || bKeyLayoutAsked || !PC || !Bindings || Bindings->HasChosenLayout())
	{
		return false;
	}
	// Die Kartentests (RTS.StartLoadout.AutoPick) laufen ohne Klick durch.
	const IConsoleVariable* AutoPick = IConsoleManager::Get().FindConsoleVariable(TEXT("RTS.StartLoadout.AutoPick"));
	if (AutoPick && AutoPick->GetInt() >= 0)
	{
		return false;
	}
	bKeyLayoutAsked = true;
	const AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(PC->GetPawn());
	return URTSKeyLayoutChoiceWidget::Open(PC, Camera ? Camera->GetKeyboardWidgetClass() : nullptr, CardMaterial, true, MoveTemp(Then)) != nullptr;
}

void UStartLoadoutPlayerComponent::ShowDifficultyWidget(const TArray<FStartLoadoutDifficulty>& Levels, int32 DefaultIndex, const FText& Header, UMaterialInterface* CardMaterial)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	if (DifficultyWidget)
	{
		DifficultyWidget->RemoveFromParent();
	}
	DifficultyWidget = CreateWidget<UStartLoadoutDifficultyWidget>(PC, UStartLoadoutDifficultyWidget::StaticClass());
	if (DifficultyWidget)
	{
		DifficultyWidget->Setup(this, Levels, DefaultIndex, Header, CardMaterial);
		DifficultyWidget->AddToViewport(50);
		PC->SetShowMouseCursor(true);
	}
}

UStartLoadoutPlayerComponent::UStartLoadoutPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UStartLoadoutPlayerComponent* UStartLoadoutPlayerComponent::FindOrAdd(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}
	if (UStartLoadoutPlayerComponent* Existing = PC->FindComponentByClass<UStartLoadoutPlayerComponent>())
	{
		return Existing;
	}
	UStartLoadoutPlayerComponent* Comp = NewObject<UStartLoadoutPlayerComponent>(PC, TEXT("StartLoadoutPlayer"));
	Comp->SetIsReplicated(true);
	Comp->RegisterComponent();
	PC->AddInstanceComponent(Comp);
	return Comp;
}

void UStartLoadoutPlayerComponent::Client_OfferLoadout_Implementation(TSubclassOf<UStartLoadoutWidget> InWidgetClass,
	const FStartLoadoutFaction& Faction, const TArray<FName>& RowNames, const TArray<FStartLoadoutSetRow>& Sets, float Seconds, const FText& Header, const FText& Hint)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	TSubclassOf<UStartLoadoutWidget> Class = InWidgetClass ? InWidgetClass : TSubclassOf<UStartLoadoutWidget>(UStartLoadoutWidget::StaticClass());
	Widget = CreateWidget<UStartLoadoutWidget>(PC, Class);
	if (!Widget)
	{
		return;
	}
	if (!Header.IsEmpty())
	{
		Widget->HeaderText = Header;
	}
	Widget->Setup(this, Faction, RowNames, Sets, Seconds, Hint);
	Widget->AddToViewport(50);
	PC->SetShowMouseCursor(true);
	// Karte ohne Schwierigkeitswahl: die Layoutwahl liegt dann ueber den Karten (sie laufen auf Zeit).
	OpenKeyLayoutChoiceFirst(Faction.CardMaterial, nullptr);
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s: selection open, %d set(s), %.0f s."), *PC->GetName(), Sets.Num(), Seconds);
}

void UStartLoadoutPlayerComponent::Client_EndSelection_Implementation(FName ChosenRow)
{
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] selection over, set %s."), *ChosenRow.ToString());
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
}

void UStartLoadoutPlayerComponent::Server_ChooseLoadout_Implementation(FName RowName)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	ARTSGameModeBase* GameMode = World ? Cast<ARTSGameModeBase>(World->GetAuthGameMode()) : nullptr;
	if (PC && GameMode && GameMode->StartLoadout)
	{
		GameMode->StartLoadout->ReceiveChoice(PC, RowName);
	}
}

void UStartLoadoutPlayerComponent::Client_OfferDifficulty_Implementation(const TArray<FStartLoadoutDifficulty>& Levels, int32 DefaultIndex, const FText& Header, UMaterialInterface* CardMaterial)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	// Noch keine Tastaturbelegung gespeichert: erst Modern / Classic / User defined, dann die Schwierigkeit.
	TWeakObjectPtr<UStartLoadoutPlayerComponent> WeakThis(this);
	TWeakObjectPtr<UMaterialInterface> WeakMaterial(CardMaterial);
	bDifficultyPending = true;
	if (OpenKeyLayoutChoiceFirst(CardMaterial, [WeakThis, Levels, DefaultIndex, Header, WeakMaterial]()
		{
			UStartLoadoutPlayerComponent* Self = WeakThis.Get();
			if (Self && Self->bDifficultyPending)
			{
				Self->bDifficultyPending = false;
				Self->ShowDifficultyWidget(Levels, DefaultIndex, Header, WeakMaterial.Get());
			}
		}))
	{
		return;
	}
	bDifficultyPending = false;
	ShowDifficultyWidget(Levels, DefaultIndex, Header, CardMaterial);
}

void UStartLoadoutPlayerComponent::Client_EndDifficulty_Implementation()
{
	bDifficultyPending = false;
	if (DifficultyWidget)
	{
		DifficultyWidget->RemoveFromParent();
		DifficultyWidget = nullptr;
	}
}

void UStartLoadoutPlayerComponent::Server_ChooseDifficulty_Implementation(int32 Index)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	ARTSGameModeBase* GameMode = World ? Cast<ARTSGameModeBase>(World->GetAuthGameMode()) : nullptr;
	if (PC && GameMode && GameMode->StartLoadout)
	{
		GameMode->StartLoadout->ReceiveDifficulty(PC, Index);
	}
}

void UStartLoadoutPlayerComponent::ChooseDifficulty(int32 Index)
{
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s sends difficulty %d."), *GetNameSafe(GetOwner()), Index);
	Server_ChooseDifficulty(Index);
}

void UStartLoadoutPlayerComponent::ChooseLoadout(FName RowName)
{
	UE_LOG(LogTemp, Log, TEXT("[StartLoadout] %s sends %s."), *GetNameSafe(GetOwner()), *RowName.ToString());
	Server_ChooseLoadout(RowName);
}

void UStartLoadoutPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
