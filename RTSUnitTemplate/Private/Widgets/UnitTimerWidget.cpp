// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/UnitTimerWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include <Components/ProgressBar.h>

void UUnitTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TimerBar->SetVisibility(ESlateVisibility::Collapsed);
	//TimerBar->SetVisibility(ESlateVisibility::Visible);
}

void UUnitTimerWidget::TimerTick()
{
	if (!OwnerCharacter.IsValid())
		return;




	AUnitBase* UnitBase = Cast<AUnitBase>(OwnerCharacter);
	
	switch (UnitBase->GetUnitState())
	{
	case UnitData::Build:
		{
			if(!DisableBuild)
			{
				
				IsVisible = true;
				AWorkingUnitBase* WorkingUnitBase = Cast<AWorkingUnitBase>(OwnerCharacter);

				if(!UnitBase || !UnitBase->UnitControlTimer || !WorkingUnitBase || !WorkingUnitBase->BuildArea || !WorkingUnitBase->BuildArea->BuildTime) return;
					
				TimerBar->SetPercent(UnitBase->UnitControlTimer / WorkingUnitBase->BuildArea->BuildTime);
				TimerBar->SetFillColorAndOpacity(BuildColor);
			}
		}
		break;
	case UnitData::ResourceExtraction:
		{
			IsVisible = true;
			AWorkingUnitBase* WorkingUnitBase = Cast<AWorkingUnitBase>(OwnerCharacter);
			TimerBar->SetPercent(UnitBase->UnitControlTimer / WorkingUnitBase->ResourceExtractionTime);
			TimerBar->SetFillColorAndOpacity(ExtractionColor);
		}
		break;
	case UnitData::Casting:
		{
			IsVisible = true;
			TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->CastTime);
			TimerBar->SetFillColorAndOpacity(CastingColor);
		}
		break;
	case UnitData::Pause:
		{
			if(!DisableAutoAttack)
			{
				IsVisible = true;
				TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->PauseDuration);
				TimerBar->SetFillColorAndOpacity(PauseColor);
			}
		}
		break;
	default:
		{
			if (UnitBase && UnitBase->CurrentUnitsLoaded > 0)
			{
				IsVisible = true;
				TimerBar->SetPercent(static_cast<float>(UnitBase->CurrentUnitsLoaded) / static_cast<float>(UnitBase->MaxTransportUnits));
				//TimerBar->SetPercent(UnitBase->MaxTransportUnits / UnitBase->CurrentUnitsLoaded);
				TimerBar->SetFillColorAndOpacity(TransportColor);
			}else
			{
				IsVisible = false;
			}
		}
		break;
	}
	


	if(!IsVisible)
	{
		TimerBar->SetVisibility(ESlateVisibility::Collapsed);
	}else if(IsVisible)
	{
		TimerBar->SetVisibility(ESlateVisibility::Visible);
	}
	
}
