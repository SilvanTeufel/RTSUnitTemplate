// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


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
