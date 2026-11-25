// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/UnitTimerWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include <Components/ProgressBar.h>
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/Image.h"
#include "Materials/MaterialInterface.h"

void UUnitTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TimerBar->SetVisibility(ESlateVisibility::Collapsed);
	if (TransportText)
	{
		TransportText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BarFrame)
	{
		BarFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	ApplyFrameStyle();
	//TimerBar->SetVisibility(ESlateVisibility::Visible);
}

void UUnitTimerWidget::SetOwnerActor(AUnitBase* Unit)
{
	OwnerCharacter = Unit;
	ApplyFrameStyle();
	GetWorld()->GetTimerManager().SetTimer(TickTimerHandle, this, &UUnitTimerWidget::TimerTick, UpdateInterval, true);
}

void UUnitTimerWidget::ApplyFrameStyle()
{
	const AUnitBase* Unit = OwnerCharacter.Get();
	if (!Unit)
	{
		return;
	}
	UMaterialInterface* Material = DefaultFrameMaterial;
	UMaterialInterface* Fill = DefaultFillMaterial;
	const FString ClassName = Unit->GetClass()->GetName();
	for (const FUnitTimerFrameStyle& Style : FrameStyles)
	{
		const bool bTag = Style.UnitTag.IsValid() && Unit->UnitTags.HasTag(Style.UnitTag);
		const bool bName = !Style.ClassNameContains.IsEmpty() && ClassName.Contains(Style.ClassNameContains);
		if (bTag || bName)
		{
			Material = Style.FrameMaterial ? Style.FrameMaterial.Get() : Material;
			Fill = Style.FillMaterial ? Style.FillMaterial.Get() : Fill;
			break;
		}
	}
	if (Material && BarFrame)
	{
		BarFrame->SetBrushFromMaterial(Material);
	}
	// The fill: a material of the faction, tinted by the state colour (FillColorAndOpacity, vertex colour).
	if (Fill && TimerBar)
	{
		FProgressBarStyle Style = TimerBar->GetWidgetStyle();
		Style.FillImage.SetResourceObject(Fill);
		Style.FillImage.DrawAs = ESlateBrushDrawType::Image;
		TimerBar->SetWidgetStyle(Style);
	}
}

void UUnitTimerWidget::TimerTick()
{
	UpdateBar();
	// The frame belongs to the bar: visible exactly while the bar is.
	if (BarFrame && TimerBar)
	{
		const bool bBarShown = TimerBar->GetVisibility() != ESlateVisibility::Collapsed && TimerBar->GetVisibility() != ESlateVisibility::Hidden;
		BarFrame->SetVisibility(bBarShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UUnitTimerWidget::UpdateBar()
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
