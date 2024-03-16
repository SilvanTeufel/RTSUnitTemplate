// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/LevelUnit.h"

#include "Core/TalentSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

void ALevelUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RegenerationTimer += DeltaTime;

	if(RegenerationTimer >= RegenerationDelayTime)
	{

		if(Attributes->GetHealth() > 0)
		{
			Attributes->SetAttributeHealth(Attributes->GetHealth()+Attributes->GetHealthRegeneration());
			Attributes->SetAttributeShield(Attributes->GetShield()+Attributes->GetShieldRegeneration());
		}
		//RegenerationTimer = 0.f;

		if(AutoLeveling && HasAuthority()) AutoLevelUp();

		//UE_LOG(LogTemp, Log, TEXT("ALevelUnit LevelData.CharacterLevel: %d"), LevelData.CharacterLevel);
	}
	
}

void ALevelUnit::BeginPlay()
{
	Super::BeginPlay();
}

void ALevelUnit::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ALevelUnit, LevelData);
	DOREPLIFETIME(ALevelUnit, LevelUpData);
	DOREPLIFETIME(ALevelUnit, StaminaInvestmentEffect);
	DOREPLIFETIME(ALevelUnit, AttackPowerInvestmentEffect);
	DOREPLIFETIME(ALevelUnit, WillpowerInvestmentEffect);
	DOREPLIFETIME(ALevelUnit, HasteInvestmentEffect);
	DOREPLIFETIME(ALevelUnit, ArmorInvestmentEffect);
	DOREPLIFETIME(ALevelUnit, MagicResistanceInvestmentEffect);
	DOREPLIFETIME(ALevelUnit, CustomEffects);
	DOREPLIFETIME(ALevelUnit, UnitIndex);

	
}

void ALevelUnit::SetUnitIndex(int32 NewIndex)
{
	UnitIndex = NewIndex;
}


void ALevelUnit::LevelUp_Implementation()
{
	//UE_LOG(LogTemp, Log, TEXT("Before Level Up: Level %d, Experience %d"), LevelData.CharacterLevel, LevelData.Experience);

	if(LevelData.CharacterLevel < LevelUpData.MaxCharacterLevel && LevelData.Experience > LevelUpData.ExperiencePerLevel*LevelData.CharacterLevel)
	{
		LevelData.CharacterLevel++;
		LevelData.TalentPoints += LevelUpData.TalentPointsPerLevel; // Define TalentPointsPerLevel as appropriate
		LevelData.Experience -= LevelUpData.ExperiencePerLevel*LevelData.CharacterLevel;
		// Trigger any additional level-up effects or logic here
	}

	//UE_LOG(LogTemp, Log, TEXT("After Level Up: Level %d, Experience %d"), LevelData.CharacterLevel, LevelData.Experience);
}

void ALevelUnit::AutoLevelUp()
{
	LevelUp();

	for(int i = 0; i < AutolevelConfig[0]; i++)
		InvestPointIntoStamina();
	
	for(int i = 0; i < AutolevelConfig[1]; i++)
		InvestPointIntoAttackPower();
	
	for(int i = 0; i < AutolevelConfig[2]; i++)
		InvestPointIntoWillPower();
	
	for(int i = 0; i < AutolevelConfig[3]; i++)
		InvestPointIntoHaste();
	
	for(int i = 0; i < AutolevelConfig[4]; i++)
		InvestPointIntoArmor();

	for(int i = 0; i < AutolevelConfig[5]; i++)
		InvestPointIntoMagicResistance();
}

void ALevelUnit::SetLevel(int32 CharLevel)
{
	if (CharLevel < 1) 
	{
		CharLevel = 1;  // Ensures level is not set to below 1
	}

	// Calculate the difference in levels
	int32 LevelDifference = CharLevel - LevelData.CharacterLevel;

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
	if (LevelData.TalentPoints > 0 && StaminaInvestmentEffect && Attributes->GetStamina() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyInvestmentEffect(StaminaInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
	
}

void ALevelUnit::InvestPointIntoAttackPower()
{
	if (LevelData.TalentPoints > 0 && AttackPowerInvestmentEffect && Attributes->GetAttackPower() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyInvestmentEffect(AttackPowerInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}


void ALevelUnit::InvestPointIntoWillPower()
{
	if (LevelData.TalentPoints > 0 && WillpowerInvestmentEffect && Attributes->GetWillpower() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyInvestmentEffect(WillpowerInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoHaste()
{
	if (LevelData.TalentPoints > 0 && HasteInvestmentEffect && Attributes->GetHaste() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyInvestmentEffect(HasteInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoArmor()
{
	if (LevelData.TalentPoints > 0 && ArmorInvestmentEffect && Attributes->GetArmor() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyInvestmentEffect(ArmorInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoMagicResistance()
{
	if (LevelData.TalentPoints > 0 && MagicResistanceInvestmentEffect && Attributes->GetMagicResistance() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyInvestmentEffect(MagicResistanceInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}


void ALevelUnit::ResetTalents()
{
	LevelData.TalentPoints = LevelData.TalentPoints+LevelData.UsedTalentPoints;
	LevelData.UsedTalentPoints = 0;
	//InitializeAttributes();
	
	Attributes->SetStamina(0);
	Attributes->SetMaxHealth(Attributes->GetBaseHealth());
	Attributes->SetAttackPower(0);
	Attributes->SetAttackDamage(Attributes->GetBaseAttackDamage());
	Attributes->SetWillpower(0);
	Attributes->SetHaste(0);
	Attributes->SetRunSpeed(Attributes->GetBaseRunSpeed());
	Attributes->SetArmor(0);
	Attributes->SetMagicResistance(0);

	Attributes->SetHealthRegeneration(0);
	Attributes->SetShieldRegeneration(0);
}

void ALevelUnit::ResetLevel()
{
	LevelData.CharacterLevel = 1;
	LevelData.TalentPoints = 0;
	LevelData.UsedTalentPoints = 0;
	LevelData.Experience = 0;
	//InitializeAttributes();
	Attributes->SetStamina(0);
	Attributes->SetMaxHealth(Attributes->GetBaseHealth());
	Attributes->SetAttackPower(0);
	Attributes->SetAttackDamage(Attributes->GetBaseAttackDamage());
	Attributes->SetWillpower(0);
	Attributes->SetHaste(0);
	Attributes->SetRunSpeed(Attributes->GetBaseRunSpeed());
	Attributes->SetArmor(0);
	Attributes->SetMagicResistance(0);

	Attributes->SetHealthRegeneration(0);
	Attributes->SetShieldRegeneration(0);
}

void ALevelUnit::ApplyInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect)
{
	//UE_LOG(LogTemp, Warning, TEXT("ApplyTalentPointInvestmentEffect!"));
	if (AbilitySystemComponent && InvestmentEffect)
	{
		//UE_LOG(LogTemp, Warning, TEXT("ApplyTalentPointInvestmentEffect!2"));
		AbilitySystemComponent->ApplyGameplayEffectToSelf(InvestmentEffect.GetDefaultObject(), 1, AbilitySystemComponent->MakeEffectContext());
	}
}


void ALevelUnit::SaveLevelDataAndAttributes(const FString& SlotName)
{
	UTalentSaveGame* SaveGameInstance = Cast<UTalentSaveGame>(UGameplayStatics::CreateSaveGameObject(UTalentSaveGame::StaticClass()));

	if (SaveGameInstance)
	{
		SaveGameInstance->LevelData = LevelData;
		SaveGameInstance->LevelUpData = LevelUpData;
		SaveGameInstance->PopulateAttributeSaveData(Attributes);
		UGameplayStatics::SaveGameToSlot(SaveGameInstance, SlotName, 0);
	}
}

void ALevelUnit::LoadLevelDataAndAttributes(const FString& SlotName)
{
	
	UTalentSaveGame* SaveGameInstance = Cast<UTalentSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));

	if (SaveGameInstance)
	{
		LevelData = SaveGameInstance->LevelData;
		LevelUpData = SaveGameInstance->LevelUpData;
		
		Attributes->UpdateAttributes(SaveGameInstance->AttributeSaveData);
	}
	
}
