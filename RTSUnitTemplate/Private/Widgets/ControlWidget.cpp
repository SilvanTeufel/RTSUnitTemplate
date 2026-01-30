// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/ControlWidget.h"
#include "Components/CheckBox.h"
#include "Characters/Camera/CameraBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Kismet/GameplayStatics.h"

void UControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SwapScrollCheckBox)
	{
		SwapScrollCheckBox->OnCheckStateChanged.AddDynamic(this, &UControlWidget::OnSwapScrollChanged);

		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			ACameraBase* Camera = Cast<ACameraBase>(PC->GetPawn());
			if (Camera)
			{
				SwapScrollCheckBox->SetIsChecked(Camera->SwapScroll);
			}
		}
	}

	if (SwapAttackMoveCheckBox)
	{
		SwapAttackMoveCheckBox->OnCheckStateChanged.AddDynamic(this, &UControlWidget::OnSwapAttackMoveChanged);

		ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayer());
		if (PC)
		{
			SwapAttackMoveCheckBox->SetIsChecked(PC->SwapAttackMove);
		}
	}
}

void UControlWidget::OnSwapScrollChanged(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ACameraBase* Camera = Cast<ACameraBase>(PC->GetPawn());
		if (Camera)
		{
			Camera->SwapScroll = bIsChecked;
		}
	}
}

void UControlWidget::OnSwapAttackMoveChanged(bool bIsChecked)
{
	ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayer());
	if (PC)
	{
		PC->SwapAttackMove = bIsChecked;
	}
}
