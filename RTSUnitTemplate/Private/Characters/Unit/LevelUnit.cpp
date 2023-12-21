// LevelUnit.cpp

#include "Characters/Unit/LevelUnit.h"

#include "Core/TalentSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
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
		//UE_LOG(LogTemp, Warning, TEXT("GetHealthRegeneration! %f"), Attributes->GetHealthRegeneration());
		//UE_LOG(LogTemp, Warning, TEXT("GetWillpower! %f"), Attributes->GetWillpower());
		Attributes->SetAttributeHealth(Attributes->GetHealth()+Attributes->GetHealthRegeneration());
		Attributes->SetAttributeShield(Attributes->GetShield()+Attributes->GetShieldRegeneration());
		RegenerationTimer = 0.f;
		//if(HasAuthority())UE_LOG(LogTemp, Warning, TEXT("SERVER LevelUnitBase->Attributes! %f"), Attributes->GetAttackDamage());
		//if(!HasAuthority())UE_LOG(LogTemp, Warning, TEXT("CLIENT LevelUnitBase->Attributes! %f"), Attributes->GetAttackDamage());


		if(AutoLeveling && HasAuthority()) AutoLevelUp();
		//SetOwner(GetController());
		//if(HasAuthority())HandleInvestment(CurrentInvestmentState);
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
	DOREPLIFETIME(ALevelUnit, UnitIndex);
}

void ALevelUnit::SetUnitIndex(int32 NewIndex)
{
	UnitIndex = NewIndex;
}


void ALevelUnit::LevelUp_Implementation()
{
	if(LevelData.CharacterLevel < LevelUpData.MaxCharacterLevel && LevelData.Experience > LevelUpData.ExperiencePerLevel*LevelData.CharacterLevel)
	{
		LevelData.CharacterLevel++;
		LevelData.TalentPoints += LevelUpData.TalentPointsPerLevel; // Define TalentPointsPerLevel as appropriate
		LevelData.Experience -= LevelUpData.ExperiencePerLevel*LevelData.CharacterLevel;
		// Trigger any additional level-up effects or logic here
	}
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
		ApplyTalentPointInvestmentEffect(StaminaInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
	
}

void ALevelUnit::InvestPointIntoAttackPower()
{
	if (LevelData.TalentPoints > 0 && AttackPowerInvestmentEffect && Attributes->GetAttackPower() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(AttackPowerInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}


void ALevelUnit::InvestPointIntoWillPower()
{
	if (LevelData.TalentPoints > 0 && WillpowerInvestmentEffect && Attributes->GetWillpower() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(WillpowerInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoHaste()
{
	if (LevelData.TalentPoints > 0 && HasteInvestmentEffect && Attributes->GetHaste() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(HasteInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoArmor()
{
	if (LevelData.TalentPoints > 0 && ArmorInvestmentEffect && Attributes->GetArmor() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(ArmorInvestmentEffect);
		--LevelData.TalentPoints; // Deduct a talent point
		LevelData.UsedTalentPoints++;
	}
}

void ALevelUnit::InvestPointIntoMagicResistance()
{
	if (LevelData.TalentPoints > 0 && MagicResistanceInvestmentEffect && Attributes->GetMagicResistance() < LevelUpData.MaxTalentsPerStat)
	{
		ApplyTalentPointInvestmentEffect(MagicResistanceInvestmentEffect);
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
}

void ALevelUnit::ResetLevel()
{
	LevelData.CharacterLevel = 1;
	LevelData.TalentPoints = 0;
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

void ALevelUnit::ApplyTalentPointInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect)
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
		SaveGameInstance->Attributes = Attributes;
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
		UpdateAttributes(SaveGameInstance->Attributes);
	}
	
}

void ALevelUnit::UpdateAttributes(UAttributeSetBase* LoadedAttributes)
{
	if (LoadedAttributes && Attributes)
	{
		Attributes->SetHealth(LoadedAttributes->GetHealth());
		Attributes->SetMaxHealth(LoadedAttributes->GetMaxHealth());
		Attributes->SetHealthRegeneration(LoadedAttributes->GetHealthRegeneration());
		Attributes->SetShield(LoadedAttributes->GetShield());
		Attributes->SetMaxShield(LoadedAttributes->GetMaxShield());
		Attributes->SetShieldRegeneration(LoadedAttributes->GetShieldRegeneration());
		Attributes->SetAttackDamage(LoadedAttributes->GetAttackDamage());
		Attributes->SetRange(LoadedAttributes->GetRange());
		Attributes->SetRunSpeed(LoadedAttributes->GetRunSpeed());
		Attributes->SetIsAttackedSpeed(LoadedAttributes->GetIsAttackedSpeed());
		Attributes->SetRunSpeedScale(LoadedAttributes->GetRunSpeedScale());
		Attributes->SetProjectileScaleActorDirectionOffset(LoadedAttributes->GetProjectileScaleActorDirectionOffset());
		Attributes->SetProjectileSpeed(LoadedAttributes->GetProjectileSpeed());
		Attributes->SetStamina(LoadedAttributes->GetStamina());
		Attributes->SetAttackPower(LoadedAttributes->GetAttackPower());
		Attributes->SetWillpower(LoadedAttributes->GetWillpower());
		Attributes->SetHaste(LoadedAttributes->GetHaste());
		Attributes->SetArmor(LoadedAttributes->GetArmor());
		Attributes->SetMagicResistance(LoadedAttributes->GetMagicResistance());

		// Notify the Ability System Component about the attribute changes
		AbilitySystemComponent->ForceReplication();
	}
}