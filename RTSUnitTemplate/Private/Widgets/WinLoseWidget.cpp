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
	if (PC)
	{
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeUIOnly());
	}
}

void UWinLoseWidget::OnOkClicked()
{
	if (OkButton)
	{
		OkButton->SetIsEnabled(false);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UGameSaveSubsystem>())
		{
			SaveSubsystem->SetPendingQuickSave(true);
		}

		if (UMapSwitchSubsystem* MapSwitchSub = GI->GetSubsystem<UMapSwitchSubsystem>())
		{
			if (bWon && !TargetMapName.IsEmpty() && DestinationSwitchTagToEnable != NAME_None)
			{
				MapSwitchSub->MarkSwitchEnabledForMap(TargetMapName, DestinationSwitchTagToEnable);
			}
		}
	}

	ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (PC && !TargetMapName.IsEmpty())
	{
		PC->Server_TravelToMap(TargetMapName);
	}
}
