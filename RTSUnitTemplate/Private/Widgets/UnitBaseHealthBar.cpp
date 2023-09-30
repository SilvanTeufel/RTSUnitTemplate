// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/UnitBaseHealthBar.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>

void UUnitBaseHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerCharacter.IsValid())
		return;
	/*
	//OwnerCharacter->GetHealth();
	HealthBar->SetPercent(OwnerCharacter->GetHealth() / OwnerCharacter->GetMaxHealth());
	ShieldBar->SetPercent(OwnerCharacter->GetShield() / OwnerCharacter->GetMaxShield());
	
	FNumberFormattingOptions Opts;
	Opts.SetMaximumFractionalDigits(0);
	
	CurrentHealthLabel->SetText(FText::AsNumber(OwnerCharacter->GetHealth(), &Opts));
	MaxHealthLabel->SetText(FText::AsNumber(OwnerCharacter->GetMaxHealth(), &Opts));
	
	CurrentShieldLabel->SetText(FText::AsNumber(OwnerCharacter->GetShield(), &Opts));
	MaxShieldLabel->SetText(FText::AsNumber(OwnerCharacter->GetMaxShield(), &Opts));*/

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

	// Only update visibility if the shield value has changed
	if(CurrentShieldValue != PreviousShieldValue)
	{
		if(CurrentShieldValue == 0)
		{
			ShieldBar->SetVisibility(ESlateVisibility::Collapsed);
			CurrentShieldLabel->SetVisibility(ESlateVisibility::Collapsed);
			MaxShieldLabel->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ShieldBar->SetVisibility(ESlateVisibility::Visible);
			CurrentShieldLabel->SetVisibility(ESlateVisibility::Visible);
			MaxShieldLabel->SetVisibility(ESlateVisibility::Visible);
		}
		
		// Update the previous shield value
		PreviousShieldValue = CurrentShieldValue;
	}
}
