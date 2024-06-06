// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/SpawnEnergyBar.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>

void USpawnEnergyBar::TimerTick()
{
	if (!OwnerPlatform)
		return;
	
	// Update Health values
	EnergyBar->SetPercent(OwnerPlatform->GetEnergy() / OwnerPlatform->GetMaxEnergy());
	FNumberFormattingOptions Opts;
	Opts.SetMaximumFractionalDigits(0);
	CurrentEnergyLabel->SetText(FText::AsNumber(OwnerPlatform->GetEnergy(), &Opts));
	MaxEnergyLabel->SetText(FText::AsNumber(OwnerPlatform->GetMaxEnergy(), &Opts));
	

}
