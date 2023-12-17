// LevelUnit.cpp

#include "Characters/Unit/LevelUnit.h"
/*
ALevelUnit::ALevelUnit()
{
	// Initialize default values
	CharacterLevel = 1; // Start at level 1
	TalentPoints = 0; // Initial talent points can be set here or through another method
}*/

void ALevelUnit::LevelUp()
{
	CharacterLevel++;
	TalentPoints += TalentPointsPerLevel; // Define TalentPointsPerLevel as appropriate
	// Trigger any additional level-up effects or logic here
}

void ALevelUnit::InvestPointIntoStamina()
{
	if (TalentPoints > 0 && StaminaInvestmentEffect)
	{
		ApplyTalentPointInvestmentEffect(StaminaInvestmentEffect);
		--TalentPoints; // Deduct a talent point
	}
}

void ALevelUnit::InvestPointIntoAttackPower()
{
	if (TalentPoints > 0 && AttackPowerInvestmentEffect)
	{
		ApplyTalentPointInvestmentEffect(AttackPowerInvestmentEffect);
		--TalentPoints; // Deduct a talent point
	}
}

void ALevelUnit::ApplyTalentPointInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect)
{
	if (AbilitySystemComponent && InvestmentEffect)
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(InvestmentEffect.GetDefaultObject(), 1, AbilitySystemComponent->MakeEffectContext());
	}
}