// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Widgets/UnitBaseHealthBar.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>

void UUnitBaseHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerCharacter)
		return;

	// Update Health values
	HealthBar->SetPercent(OwnerCharacter->Attributes->GetHealth() / OwnerCharacter->Attributes->GetMaxHealth());
	FNumberFormattingOptions Opts;
	Opts.SetMaximumFractionalDigits(0);
	CurrentHealthLabel->SetText(FText::AsNumber(OwnerCharacter->Attributes->GetHealth(), &Opts));
	MaxHealthLabel->SetText(FText::AsNumber(OwnerCharacter->Attributes->GetMaxHealth(), &Opts));
	
	// Update Shield values
	float CurrentShieldValue = OwnerCharacter->Attributes->GetShield();
	ShieldBar->SetPercent(CurrentShieldValue / OwnerCharacter->Attributes->GetMaxShield());
	CurrentShieldLabel->SetText(FText::AsNumber(CurrentShieldValue, &Opts));
	MaxShieldLabel->SetText(FText::AsNumber(OwnerCharacter->Attributes->GetMaxShield(), &Opts));
	CharacterLevel->SetText(FText::AsNumber(OwnerCharacter->LevelData.CharacterLevel, &Opts));
	UpdateExperience();
}

void UUnitBaseHealthBar::UpdateExperience()
{
	if(!OwnerCharacter) return;
    
	if(ExperienceProgressBar)
	{
		// Ensure Experience and CharacterLevel are not causing division by zero
		if (OwnerCharacter->LevelData.Experience && OwnerCharacter->LevelData.CharacterLevel)
		{
			float N = (float)(OwnerCharacter->LevelUpData.ExperiencePerLevel * OwnerCharacter->LevelData.CharacterLevel);
			float Z = (float)OwnerCharacter->LevelData.Experience;
			//if(N > 0 && Z > 0 && Z < N)
			ExperienceProgressBar->SetPercent(Z / N);
		}
	}
}