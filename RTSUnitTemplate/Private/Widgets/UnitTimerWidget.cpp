// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/UnitTimerWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include <Components/ProgressBar.h>
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UUnitTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TimerBar->SetVisibility(ESlateVisibility::Collapsed);
	if (TransportText)
	{
		TransportText->SetVisibility(ESlateVisibility::Collapsed);
	}
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
				
				MyWidgetIsVisible = true;
				AWorkingUnitBase* WorkingUnitBase = Cast<AWorkingUnitBase>(OwnerCharacter);

				if(!UnitBase || !UnitBase->UnitControlTimer || !WorkingUnitBase || !WorkingUnitBase->BuildArea || !WorkingUnitBase->BuildArea->BuildTime) return;
					
				TimerBar->SetPercent(UnitBase->UnitControlTimer / WorkingUnitBase->BuildArea->BuildTime);
				TimerBar->SetFillColorAndOpacity(BuildColor);
			}
		}
		break;
	case UnitData::ResourceExtraction:
		{
			MyWidgetIsVisible = true;
			AWorkingUnitBase* WorkingUnitBase = Cast<AWorkingUnitBase>(OwnerCharacter);
			TimerBar->SetPercent(UnitBase->UnitControlTimer / WorkingUnitBase->ResourceExtractionTime);
			TimerBar->SetFillColorAndOpacity(ExtractionColor);
		}
		break;
	case UnitData::Casting:
		{
			// Compute percent safely and auto-hide when casting complete on client
			float Denom = UnitBase->CastTime;
			float Percent = (Denom > KINDA_SMALL_NUMBER) ? (UnitBase->UnitControlTimer / Denom) : 1.f;
			Percent = FMath::Clamp(Percent, 0.f, 1.f);
			TimerBar->SetPercent(Percent);
			TimerBar->SetFillColorAndOpacity(CastingColor);
			// Visible only while cast is in progress and has actually started
			MyWidgetIsVisible = (Percent > 0.f && Percent < 1.f);
		}
		break;
	case UnitData::Repair:
		{
			// Compute percent safely and auto-hide when casting complete on client
			float Denom = UnitBase->CastTime;
			float Percent = (Denom > KINDA_SMALL_NUMBER) ? (UnitBase->UnitControlTimer / Denom) : 1.f;
			Percent = FMath::Clamp(Percent, 0.f, 1.f);
			TimerBar->SetPercent(Percent);
			TimerBar->SetFillColorAndOpacity(CastingColor);
			// Visible only while cast is in progress and has actually started
			MyWidgetIsVisible = (Percent > 0.f && Percent < 1.f);
		}
		break;
	case UnitData::Pause:
		{
			if(!DisableAutoAttack)
			{
				MyWidgetIsVisible = true;
				TimerBar->SetPercent(UnitBase->UnitControlTimer / UnitBase->PauseDuration);
				TimerBar->SetFillColorAndOpacity(PauseColor);
			}
		}
		break;
	default:
		{
			if (UnitBase && UnitBase->IsATransporter)
			{
				// Show transport load progress on clients too, independent of server toggles
				const float Denom = (UnitBase->MaxTransportUnits > 0) ? static_cast<float>(UnitBase->MaxTransportUnits) : 1.f;
				const float Percent = FMath::Clamp(static_cast<float>(UnitBase->CurrentUnitsLoaded) / Denom, 0.f, 1.f);
				TimerBar->SetPercent(Percent);
				TimerBar->SetFillColorAndOpacity(TransportColor);
				MyWidgetIsVisible = (UnitBase->CurrentUnitsLoaded > 0);

				if (TransportText)
				{
					const FString LoadString = FString::Printf(TEXT("%d/%d"), UnitBase->CurrentUnitsLoaded, UnitBase->MaxTransportUnits);
					TransportText->SetText(FText::FromString(LoadString));
				}
			}
			else
			{
				MyWidgetIsVisible = false;
			}
		}
		break;
	}
	


	if(!MyWidgetIsVisible)
	{
		TimerBar->SetVisibility(ESlateVisibility::Collapsed);
		if (TransportText) TransportText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		if (UnitBase && UnitBase->IsATransporter && TransportText)
		{
			TransportText->SetVisibility(ESlateVisibility::Visible);
			TimerBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			TimerBar->SetVisibility(ESlateVisibility::Visible);
			if (TransportText) TransportText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	
}
