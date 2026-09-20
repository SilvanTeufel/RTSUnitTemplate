// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/LevelUnit.h"
#include "GameStates/UpgradeGameState.h"

#include "Core/TalentSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ScopeExit.h"   // ON_SCOPE_EXIT in ApplyInvestmentEffect

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

	// Nachvergabe: was das Team laengst gekauft hat, gilt auch fuer eine Einheit, die erst
	// mitten in der Partie dazukommt. Ueber den Timer, weil die Teamnummer hier oft noch
	// nicht steht.
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(AttributeTreeSyncTimer, this,
			&ALevelUnit::RetrySyncAttributeTreeFromTeam, 0.5f, /*bLoop=*/true, /*FirstDelay=*/0.25f);
	}
}

void ALevelUnit::RetrySyncAttributeTreeFromTeam()
{
	--AttributeTreeSyncAttemptsLeft;

	const bool bGiveUp = (AttributeTreeSyncAttemptsLeft <= 0);
	if (TeamId >= 1 || bGiveUp)
	{
		GetWorldTimerManager().ClearTimer(AttributeTreeSyncTimer);
		if (TeamId >= 1)
		{
			SyncAttributeTreeFromTeam();
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Attributbaum] '%s' hat nach 20 Versuchen keine Teamnummer - keine Nachvergabe."),
				*GetName());
		}
	}
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
	// KEINE Vorratspruefung mehr: ob bezahlt werden kann, entscheidet der Teamtopf im
	// AUpgradeGameState. Diese Funktion beantwortet nur noch "passt der Knoten zu DIESER
	// Einheit und ist er noch nicht voll".
	return true;
}

bool ALevelUnit::HasInvestableAttributeTreeNode() const
{
	if (!AttributeTreeDataTable)
	{
		return false;
	}

	// CanInvestInAttributeTreeNode prueft Tag-Zugehoerigkeit, Knotenobergrenze, Freischaltung
	// und Vorrat in einem - hier genuegt deshalb der erste Treffer.
	for (const FName& NodeId : AttributeTreeDataTable->GetRowNames())
	{
		if (CanInvestInAttributeTreeNode(NodeId))
		{
			return true;
		}
	}
	return false;
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

bool ALevelUnit::ApplyAttributeTreeEffectOnly(EAttributeTreeStat Stat)
{
	if (!Attributes)
	{
		return false;
	}

	const int32 Deckel = LevelUpData.MaxTalentsPerStat;

	switch (Stat)
	{
	case EAttributeTreeStat::Stamina:
		if (StaminaInvestmentEffect && Attributes->GetStamina() < Deckel)
		{
			ApplyInvestmentEffect(StaminaInvestmentEffect);
			return true;
		}
		break;
	case EAttributeTreeStat::AttackPower:
		if (AttackPowerInvestmentEffect && Attributes->GetAttackPower() < Deckel)
		{
			ApplyInvestmentEffect(AttackPowerInvestmentEffect);
			return true;
		}
		break;
	case EAttributeTreeStat::Willpower:
		if (WillpowerInvestmentEffect && Attributes->GetWillpower() < Deckel)
		{
			ApplyInvestmentEffect(WillpowerInvestmentEffect);
			return true;
		}
		break;
	case EAttributeTreeStat::Haste:
		if (HasteInvestmentEffect && Attributes->GetHaste() < Deckel)
		{
			ApplyInvestmentEffect(HasteInvestmentEffect);
			return true;
		}
		break;
	case EAttributeTreeStat::Armor:
		if (ArmorInvestmentEffect && Attributes->GetArmor() < Deckel)
		{
			ApplyInvestmentEffect(ArmorInvestmentEffect);
			return true;
		}
		break;
	case EAttributeTreeStat::MagicResistance:
		if (MagicResistanceInvestmentEffect && Attributes->GetMagicResistance() < Deckel)
		{
			ApplyInvestmentEffect(MagicResistanceInvestmentEffect);
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

// ENTFERNT am 20.09.2026: ALevelUnit::InvestInAttributeTreeNode.
//
// Investiert wird nur noch ueber das Team - AExtendedCameraBase::Server_InvestTeamAttributeTreeNode
// bucht im AUpgradeGameState und ruft danach ApplyAttributeTreeNodeFromTeam auf jeder passenden
// Einheit auf. Ein zweiter Investitionsweg mit eigenem Vorrat wuerde wieder auseinanderlaufen.
bool ALevelUnit::ApplyAttributeTreeNodeFromTeam(FName NodeId)
{
	if (!AttributeTreeDataTable || NodeId.IsNone())
	{
		return false;
	}

	const FAttributeTreeNodeRow* Row = AttributeTreeDataTable->FindRow<FAttributeTreeNodeRow>(
		NodeId, TEXT("ApplyAttributeTreeNodeFromTeam"), /*bWarnIfMissing=*/false);
	if (!Row)
	{
		return false;
	}

	// Der Tag entscheidet, WER die Aufwertung bekommt - ein Knoten ohne Tag gilt fuer alle.
	if (!DoesAttributeTreeNodeMatchUnit(*Row))
	{
		return false;
	}

	// Freischaltung wird hier NICHT geprueft: das Team hat den Knoten bereits bezahlt, und die
	// Vorbedingung gilt fuer den Baum des Teams, nicht fuer diese Einheit. Wer hier noch einmal
	// pruefte, wuerde neu gespawnte Einheiten aussperren, deren eigener Baum noch leer ist.
	ApplyAttributeTreeEffectOnly(Row->Attribute);

	for (FAttributeTreeNodeState& State : AttributeTreeNodes)
	{
		if (State.NodeId == NodeId)
		{
			State.Points++;
			return true;
		}
	}

	FAttributeTreeNodeState NewState;
	NewState.NodeId = NodeId;
	NewState.Points = 1;
	AttributeTreeNodes.Add(NewState);
	return true;
}

void ALevelUnit::SyncAttributeTreeFromTeam()
{
	if (!HasAuthority() || !AttributeTreeDataTable)
	{
		return;
	}

	const UWorld* Welt = GetWorld();
	AUpgradeGameState* GameStateRef = Welt ? Welt->GetGameState<AUpgradeGameState>() : nullptr;
	if (!GameStateRef)
	{
		return;
	}

	int32 Nachgeholt = 0;
	const TArray<FAttributeTreeNodeState> TeamNodes = GameStateRef->GetTeamAttributeTreeNodes(TeamId);
	for (const FAttributeTreeNodeState& TeamNode : TeamNodes)
	{
		const int32 Fehlend = TeamNode.Points - GetAttributeTreeNodePoints(TeamNode.NodeId);
		for (int32 i = 0; i < Fehlend; ++i)
		{
			if (!ApplyAttributeTreeNodeFromTeam(TeamNode.NodeId))
			{
				break; // Tag passt nicht - die restlichen Stufen desselben Knotens auch nicht.
			}
			++Nachgeholt;
		}
	}

	if (Nachgeholt > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Attributbaum] Nachvergabe an '%s' (Team %d): %d Stufen aus %d Teamknoten."),
			*GetName(), TeamId, Nachgeholt, TeamNodes.Num());
	}
}

void ALevelUnit::ResetAttributeTree()
{
	AttributeTreeNodes.Empty();

	// Die Erstattung liegt beim Team (AUpgradeGameState::ResetTeamAttributeTree) - hier wird
	// nur noch die Wirkung auf DIESER Einheit geloescht.

	// ResetTalents setzt die ATTRIBUTE auf null und erstattet die Talentpunkte. Beides gehoert
	// zusammen: Baum und TalentChooser schreiben in dieselben Attribute, ohne dass irgendwo steht,
	// welcher Punkt welchen Anteil gestellt hat. Wer die Attribute leert, muss also auch die
	// Talentpunkte erstatten, sonst waeren sie ersatzlos verloren.
	ResetTalents();
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
	if (AbilitySystemComponent && InvestmentEffect)
	{
		// Siehe ActiveInvestments: markiert die Aenderung als gewollt, damit die Healthbars
		// nicht darauf anspringen. Der Zaehler faellt am Ende des Gueltigkeitsbereichs zurueck,
		// auch wenn der Effekt unterwegs etwas ausloest, das hier wieder hereinspringt.
		++ActiveInvestments;
		ON_SCOPE_EXIT { --ActiveInvestments; };

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
