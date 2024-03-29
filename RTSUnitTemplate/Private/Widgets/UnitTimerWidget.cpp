// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/UnitTimerWidget.h"
#include "Characters/Unit/UnitBase.h"
#include <Components/ProgressBar.h>

void UUnitTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TimerBar->SetVisibility(ESlateVisibility::Collapsed);
	
}

void UUnitTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerCharacter.IsValid())
		return;




	AUnitBase* UnitBase = Cast<AUnitBase>(OwnerCharacter);
	
	switch (UnitBase->GetUnitState())
	{
	case UnitData::Casting:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->CastTime);
			TimerBar->SetFillColorAndOpacity(CastingColor);
		}
		break;
	case UnitData::Pause:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->PauseDuration);
			TimerBar->SetFillColorAndOpacity(PauseColor);
		}
		break;
	default:
		{
			IsVisible = false;
		}
		break;
	}
	
	
	if(!IsVisible && SetVisible)
	{
		TimerBar->SetVisibility(ESlateVisibility::Collapsed);
		SetVisible = false;
	}else if(IsVisible && !SetVisible)
	{
		TimerBar->SetVisibility(ESlateVisibility::Visible);
		SetVisible = true;
	}
	
}
