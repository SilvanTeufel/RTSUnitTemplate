// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/ControlWidget.h"
#include "Components/CheckBox.h"
#include "Characters/Camera/CameraBase.h"
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
