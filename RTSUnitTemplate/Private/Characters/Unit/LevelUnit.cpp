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
		RegenerationTimer = 0.f; // Always reset timer to prevent spam

		// ONLY regenerate on the server
		if(HasAuthority() && Attributes->GetHealth() > 0)
		{
			Attributes->SetAttributeHealth(Attributes->GetHealth()+Attributes->GetHealthRegeneration());
			Attributes->SetAttributeShield(Attributes->GetShield()+Attributes->GetShieldRegeneration());
		
			if(AutoLeveling) AutoLevelUp();
		}
		//UE_LOG(LogTemp, Log, TEXT("ALevelUnit LevelData.CharacterLevel: %d"), LevelData.CharacterLevel);
	}
	
}

void ALevelUnit::BeginPlay()
{
	Super::BeginPlay();
	UpdateCachedLevelString();
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
	DOREPLIFETIME(ALevelUnit, AttributeTreeNodes);
}


void ALevelUnit::OnRep_LevelData(const FLevelData& OldLevelData)
{
	UpdateCachedLevelString();
	if (LevelData.CharacterLevel > OldLevelData.CharacterLevel)
	{
		LevelVisibilityCheck();
	}
}

void ALevelUnit::LevelVisibilityCheck()
{
	UpdateLevelUpTimestamp();
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
		UpdateCachedLevelString();
		OnLevelUp(LevelData.CharacterLevel);
		// Trigger any additional level-up effects or logic here
		LevelVisibilityCheck();
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

// ---------------------------------------------------------------------------
//  Radial Attribute Tree
// ---------------------------------------------------------------------------

int32 ALevelUnit::GetAttributeTreeNodePoints(FName NodeId) const
{
	for (const FAttributeTreeNodeState& State : AttributeTreeNodes)
	{
		if (State.NodeId == NodeId)
		{
			return State.Points;
		}
	}
	return 0;
}

const FAttributeTreeNodeRow* ALevelUnit::FindAttributeTreeRow(FName NodeId) const
{
	if (!AttributeTreeDataTable || NodeId.IsNone())
	{
		return nullptr;
	}
	return AttributeTreeDataTable->FindRow<FAttributeTreeNodeRow>(NodeId, TEXT("AttributeTree"), /*bWarnIfMissing=*/false);
}

bool ALevelUnit::DoesAttributeTreeNodeMatchUnit(const FAttributeTreeNodeRow& Row) const
{
	if (!Row.UnitTag.IsValid())
	{
		return true; // untagged node -> applies to every unit
	}
	// TalentTag / UnitTags are inherited from ASpawnerUnit and identify the unit's type.
	if (TalentTag == Row.UnitTag)
	{
		return true;
	}
	return UnitTags.HasTag(Row.UnitTag);
}

bool ALevelUnit::IsAttributeTreeNodeUnlocked(FName NodeId) const
{
	const FAttributeTreeNodeRow* Row = FindAttributeTreeRow(NodeId);
	if (!Row)
	{
		return false;
	}
	if (Row->PrevId.IsNone())
	{
		return true; // root node
	}
	const FAttributeTreeNodeRow* Parent = FindAttributeTreeRow(Row->PrevId);
	if (!Parent)
	{
		return true; // dangling parent reference -> don't hard-block the node
	}
	if (!DoesAttributeTreeNodeMatchUnit(*Parent))
	{
		return true; // parent belongs to a different unit type -> it can't gate this unit's node
	}
	return GetAttributeTreeNodePoints(Row->PrevId) >= FMath::Max(1, Parent->MaxPoints);
}

bool ALevelUnit::CanInvestInAttributeTreeNode(FName NodeId) const
{
	const FAttributeTreeNodeRow* Row = FindAttributeTreeRow(NodeId);
	if (!Row)
	{
		return false;
	}
	if (!DoesAttributeTreeNodeMatchUnit(*Row))
	{
		return false; // node belongs to a different unit type
	}
	if (GetAttributeTreeNodePoints(NodeId) >= FMath::Max(1, Row->MaxPoints))
	{
		return false;
	}
	if (!IsAttributeTreeNodeUnlocked(NodeId))
	{
		return false;
	}
	return LevelData.TalentPoints > 0;
}

bool ALevelUnit::ApplyAttributeTreeStat(EAttributeTreeStat Stat)
{
	switch (Stat)
	{
	case EAttributeTreeStat::Stamina:         InvestPointIntoStamina();         break;
	case EAttributeTreeStat::AttackPower:     InvestPointIntoAttackPower();     break;
	case EAttributeTreeStat::Willpower:       InvestPointIntoWillPower();       break;
	case EAttributeTreeStat::Haste:           InvestPointIntoHaste();           break;
	case EAttributeTreeStat::Armor:           InvestPointIntoArmor();           break;
	case EAttributeTreeStat::MagicResistance: InvestPointIntoMagicResistance(); break;
	default: return false;
	}
	return true;
}

bool ALevelUnit::InvestInAttributeTreeNode(FName NodeId)
{
	const FAttributeTreeNodeRow* Row = FindAttributeTreeRow(NodeId);
	if (!Row)
	{
		return false;
	}
	if (!DoesAttributeTreeNodeMatchUnit(*Row))
	{
		return false; // node belongs to a different unit type
	}

	const int32 MaxPts = FMath::Max(1, Row->MaxPoints);
	if (GetAttributeTreeNodePoints(NodeId) >= MaxPts)
	{
		return false; // node already full
	}
	if (!IsAttributeTreeNodeUnlocked(NodeId))
	{
		return false; // prerequisite not satisfied
	}
	if (LevelData.TalentPoints <= 0)
	{
		return false; // no talent points to spend
	}

	// Raise the stat (self-capped at MaxTalentsPerStat). InvestPointInto* consumes one talent
	// point only while the attribute is below its cap.
	const int32 PointsBefore = LevelData.TalentPoints;
	if (!ApplyAttributeTreeStat(Row->Attribute))
	{
		return false; // invalid stat enum
	}
	if (LevelData.TalentPoints >= PointsBefore)
	{
		// The attribute is already at MaxTalentsPerStat, so InvestPointInto* spent nothing. Still
		// consume one talent point so the node (and the branches it gates) can progress - the stat
		// simply stays at its maximum. This keeps deep trees, where several nodes share one capped
		// stat, from dead-locking. Raise MaxTalentsPerStat if you never want a point spent this way.
		LevelData.TalentPoints = FMath::Max(0, LevelData.TalentPoints - 1);
		LevelData.UsedTalentPoints += 1;
	}

	// Book-keeping: bump the node's invested count.
	bool bFound = false;
	for (FAttributeTreeNodeState& State : AttributeTreeNodes)
	{
		if (State.NodeId == NodeId)
		{
			State.Points++;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		FAttributeTreeNodeState NewState;
		NewState.NodeId = NodeId;
		NewState.Points = 1;
		AttributeTreeNodes.Add(NewState);
	}
	return true;
}

void ALevelUnit::ResetAttributeTree()
{
	AttributeTreeNodes.Empty();
	ResetTalents(); // refunds talent points + zeroes attributes (shared with the TalentChooser)
}

void ALevelUnit::ResetLevel()
{
	LevelData.CharacterLevel = 1;
	UpdateCachedLevelString();
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
		UpdateCachedLevelString();
		LevelUpData = SaveGameInstance->LevelUpData;
		
		Attributes->UpdateAttributes(SaveGameInstance->AttributeSaveData);
	}
	
}

void ALevelUnit::UpdateCachedLevelString()
{
	CachedLevelString = FString::FromInt(LevelData.CharacterLevel);
}
