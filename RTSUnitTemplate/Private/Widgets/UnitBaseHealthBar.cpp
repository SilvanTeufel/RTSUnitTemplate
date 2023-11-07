// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/UnitBaseHealthBar.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>

void UUnitBaseHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerCharacter.IsValid())
		return;

	// Update Health values
	HealthBar->SetPercent(OwnerCharacter->GetHealth() / OwnerCharacter->GetMaxHealth());
	FNumberFormattingOptions Opts;
	Opts.SetMaximumFractionalDigits(0);
	CurrentHealthLabel->SetText(FText::AsNumber(OwnerCharacter->GetHealth(), &Opts));
	MaxHealthLabel->SetText(FText::AsNumber(OwnerCharacter->GetMaxHealth(), &Opts));
	
	// Update Shield values
	float CurrentShieldValue = OwnerCharacter->GetShield();
	ShieldBar->SetPercent(CurrentShieldValue / OwnerCharacter->GetMaxShield());
	CurrentShieldLabel->SetText(FText::AsNumber(CurrentShieldValue, &Opts));
	MaxShieldLabel->SetText(FText::AsNumber(OwnerCharacter->GetMaxShield(), &Opts));
	
}
