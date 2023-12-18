// LevelUnit.cpp

#include "Characters/Unit/LevelUnit.h"
/*
ALevelUnit::ALevelUnit()
{
	// Initialize default values
	CharacterLevel = 1; // Start at level 1
	TalentPoints = 0; // Initial talent points can be set here or through another method
}*/

void ALevelUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RegenerationTimer += DeltaTime;

	if(RegenerationTimer >= RegenerationDelayTime)
	{
		Attributes->SetAttributeHealth(Attributes->GetHealth()+Attributes->GetHealthRegeneration());
		Attributes->SetAttributeShield(Attributes->GetShield()+Attributes->GetShieldRegeneration());
		RegenerationTimer = 0.f;

		if(AutoLeveling) AutoLevelUp();
	}

	
}

void ALevelUnit::LevelUp()
{
	if(CharacterLevel < MaxCharacterLevel && Experience > ExperiencePerLevel*CharacterLevel)
	{
		CharacterLevel++;
		TalentPoints += TalentPointsPerLevel; // Define TalentPointsPerLevel as appropriate
		Experience -= ExperiencePerLevel*CharacterLevel;
		// Trigger any additional level-up effects or logic here
	}
}

void ALevelUnit::AutoLevelUp()
{
	LevelUp();
	InvestPointIntoStamina();
	InvestPointIntoAttackPower();
	InvestPointIntoWillPower();
	InvestPointIntoHaste();
	InvestPointIntoArmor();
}

void ALevelUnit::SetLevel(int32 CharLevel)
{
	if (CharLevel < 1) 
	{
		CharLevel = 1;  // Ensures level is not set to below 1
	}

	// Calculate the difference in levels
	int32 LevelDifference = CharLevel - CharacterLevel;

	if (LevelDifference > 0) 
	{
		// Loop over the LevelUp function for each level gained
		for (int32 i = 0; i < LevelDifference; ++i)
		{
			LevelUp();
		}
	}
}

void ALevelUnit::InvestPointIntoStamina()
{
	if (TalentPoints > 0 && StaminaInvestmentEffect && Attributes->GetStamina() < MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(StaminaInvestmentEffect);
		--TalentPoints; // Deduct a talent point
		UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoAttackPower()
{
	if (TalentPoints > 0 && AttackPowerInvestmentEffect && Attributes->GetAttackPower() < MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(AttackPowerInvestmentEffect);
		--TalentPoints; // Deduct a talent point
		UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoWillPower()
{
	if (TalentPoints > 0 && WillpowerInvestmentEffect && Attributes->GetWillpower() < MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(WillpowerInvestmentEffect);
		--TalentPoints; // Deduct a talent point
		UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoHaste()
{
	if (TalentPoints > 0 && HasteInvestmentEffect && Attributes->GetHaste() < MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(HasteInvestmentEffect);
		--TalentPoints; // Deduct a talent point
		UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoArmor()
{
	if (TalentPoints > 0 && ArmorInvestmentEffect && Attributes->GetArmor() < MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(ArmorInvestmentEffect);
		--TalentPoints; // Deduct a talent point
		UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoMagicResistance()
{
	if (TalentPoints > 0 && MagicResistanceInvestmentEffect && Attributes->GetMagicResistance() < MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(MagicResistanceInvestmentEffect);
		--TalentPoints; // Deduct a talent point
		UsedTalentPoints++;
	}
}

void ALevelUnit::ResetTalents()
{
	TalentPoints = TalentPoints+UsedTalentPoints;
	UsedTalentPoints = 0;
	InitializeAttributes();
	Attributes->SetStamina(0);
	Attributes->SetAttackPower(0);
	Attributes->SetWillpower(0);
	Attributes->SetHaste(0);
	Attributes->SetArmor(0);
	Attributes->SetMagicResistance(0);
}

void ALevelUnit::ResetLevel()
{
	CharacterLevel = 1;
	TalentPoints = 0;
	UsedTalentPoints = 0;
	InitializeAttributes();
	Attributes->SetStamina(0);
	Attributes->SetAttackPower(0);
	Attributes->SetWillpower(0);
	Attributes->SetHaste(0);
	Attributes->SetArmor(0);
	Attributes->SetMagicResistance(0);
}

void ALevelUnit::ApplyTalentPointInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect)
{
	UE_LOG(LogTemp, Log, TEXT("ApplyTalentPointInvestmentEffect!"));
	if (AbilitySystemComponent && InvestmentEffect)
	{
		UE_LOG(LogTemp, Log, TEXT("ApplyTalentPointInvestmentEffect22222"));
		AbilitySystemComponent->ApplyGameplayEffectToSelf(InvestmentEffect.GetDefaultObject(), 1, AbilitySystemComponent->MakeEffectContext());
	}
}
