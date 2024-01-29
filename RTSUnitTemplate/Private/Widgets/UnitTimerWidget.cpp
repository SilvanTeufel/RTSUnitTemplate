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
	
	/*
	switch (UnitBase->GetUnitState())
	{
	case UnitData::HoldPositionAttack:
	case UnitData::Attack:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->AttackDuration);
		}
		break;
	case UnitData::HoldPositionPause:
	case UnitData::Pause:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->PauseDuration);
		}
		break;
	case UnitData::HoldPositionReload:
	case UnitData::Reload:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->ReloadTime);
		}
		break;
	case UnitData::LaserLoading:
		{
				IsVisible = true;
				TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->LaserLoadTime);
		}
	break;
	case UnitData::PrepareScatterMine:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->ScatterMineCastTime);
		}
		break;
	case UnitData::PrepareMouseBot:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->MouseBotCastTime);
		}
		break;
	default:
		{
			IsVisible = false;	
		}
		break;
		
	}*/
	
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
