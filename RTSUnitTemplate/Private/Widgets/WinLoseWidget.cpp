#include "Widgets/WinLoseWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Kismet/GameplayStatics.h"
#include "System/GameSaveSubsystem.h"
#include "System/MapSwitchSubsystem.h"
#include "Engine/GameInstance.h"

void UWinLoseWidget::SetupWidget(bool bInWon, const FString& MapName, FName InDestinationSwitchTagToEnable)
{
	bWon = bInWon;
	TargetMapName = MapName;
	DestinationSwitchTagToEnable = InDestinationSwitchTagToEnable;
	if (ResultText)
	{
		ResultText->SetText(FText::FromString(bWon ? TEXT("You Won!") : TEXT("You Lost!")));
	}
}

void UWinLoseWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (OkButton)
	{
		OkButton->OnClicked.AddUniqueDynamic(this, &UWinLoseWidget::OnOkClicked);
	}
	
	APlayerController* PC = GetOwningPlayer();
	if (PC && IsValid(PC))
	{
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeUIOnly());
	}
}

void UWinLoseWidget::OnOkClicked()
{
	if (bAlreadyClicked) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* OwningPC = GetOwningPlayer();
	if (!OwningPC || !IsValid(OwningPC))
	{
		return;
	}

	bAlreadyClicked = true;

	if (OkButton)
	{
		OkButton->SetIsEnabled(false);
	}

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(OwningPC);
	const bool bWillTravel = (PC != nullptr) && !TargetMapName.IsEmpty();

	if (UGameInstance* GI = OwningPC->GetGameInstance())
	{
		// Only arm the quick-save when we will actually travel AND only on the authority. Otherwise the
		// PostLoadMapWithWorld binding would leak (no map load to consume it) and a client would write its
		// own incomplete save during the transition. (P3/P5)
		if (bWillTravel && OwningPC->HasAuthority())
		{
			if (UGameSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UGameSaveSubsystem>())
			{
				SaveSubsystem->SetPendingQuickSave(true);
			}
		}

		if (UMapSwitchSubsystem* MapSwitchSub = GI->GetSubsystem<UMapSwitchSubsystem>())
		{
			if (bWon && !TargetMapName.IsEmpty() && DestinationSwitchTagToEnable != NAME_None)
			{
				MapSwitchSub->MarkSwitchEnabledForMap(TargetMapName, DestinationSwitchTagToEnable);
			}
		}
	}

	if (bWillTravel)
	{
		PC->Server_TravelToMap(TargetMapName, DestinationSwitchTagToEnable);
		// The pending ServerTravel will tear down this world (and this widget); nothing more to do.
		return;
	}

	// No travel target (e.g. a lose screen with no destination map): don't leave the player trapped in
	// FInputModeUIOnly behind a dead, disabled OK button. Restore normal RTS input and dismiss the widget. (P6)
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	OwningPC->SetInputMode(InputMode);
	OwningPC->SetShowMouseCursor(true);
	RemoveFromParent();
}
