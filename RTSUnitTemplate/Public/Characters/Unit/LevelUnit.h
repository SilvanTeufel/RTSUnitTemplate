// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GASUnit.h"
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "LevelUnit.generated.h"

class UTexture2D;

// ---------------------------------------------------------------------------
//  Radial Attribute Tree
//  A DataTable-driven, ring-shaped tree (alternative to the TalentChooser) that
//  invests the unit's talent points (LevelData.TalentPoints) into attributes.
// ---------------------------------------------------------------------------

/** Which unit attribute a node invests into (mirrors ALevelUnit::InvestPointInto* and UTalentChooser). */
UENUM(BlueprintType)
enum class EAttributeTreeStat : uint8
{
	Stamina         UMETA(DisplayName = "Stamina"),
	AttackPower     UMETA(DisplayName = "Attack Power"),
	Willpower       UMETA(DisplayName = "Willpower"),
	Haste           UMETA(DisplayName = "Haste"),
	Armor           UMETA(DisplayName = "Armor"),
	MagicResistance UMETA(DisplayName = "Magic Resistance")
};

/** One node of the radial attribute tree. The row NAME is the node's Id. */
USTRUCT(BlueprintType)
struct FAttributeTreeNodeRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- Graph ---
	/** Parent node's row name. NAME_None = a root node (usually Ring 0). Several rows may share a PrevId to branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Graph")
	FName PrevId = NAME_None;

	/** Ring index: 0 = centre, 1 = first ring, 2 = second ring, ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Graph")
	int32 Ring = 0;

	/** Angle on the ring in degrees. 0/360 = straight up (12 o'clock), 90 = right, 180 = down, 270 = left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Graph")
	float Angle = 0.f;

	// --- Attribute the node invests into ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Attribute")
	EAttributeTreeStat Attribute = EAttributeTreeStat::Stamina;

	/** If set, this node only applies to units whose TalentTag equals, or whose UnitTags contain, this tag.
	 *  Leave empty (invalid) to apply to every unit. Lets one tree hold unit-type-specific talents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Attribute")
	FGameplayTag UnitTag;

	// --- Investment rules ---
	/** How many points can be sunk into this node. Children unlock only once this node is full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Investment", meta = (ClampMin = "1"))
	int32 MaxPoints = 1;

	/** Reserved for future use; each invest currently spends one talent point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Investment", meta = (ClampMin = "1"))
	int32 PointCost = 1;

	// --- Presentation ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Presentation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Presentation")
	FText ToolTipTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Presentation", meta = (MultiLine = true))
	FText ToolTipText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Presentation")
	UTexture2D* Icon = nullptr;

	/** Base tint of the node disc (state shading is applied on top). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Presentation")
	FLinearColor NodeColor = FLinearColor(0.15f, 0.55f, 0.95f, 1.f);
};

/** Replicated per-node investment state for the attribute tree (NodeId = the row name). */
USTRUCT(BlueprintType)
struct FAttributeTreeNodeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Attribute Tree")
	FName NodeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute Tree")
	int32 Points = 0;
};

/**
 * ALevelUnit is a child class of AGASUnit that includes leveling and talent point functionality.
 */
UCLASS()
class RTSUNITTEMPLATE_API ALevelUnit : public AGASUnit
{
	GENERATED_BODY()

public:
	//ALevelUnit();
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, VisibleAnywhere, Category = "Leveling")
	int32 UnitIndex = INDEX_NONE;

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SetUnitIndex(int32 NewIndex);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	bool IsDoingMagicDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	bool AutoLeveling = true;
	// Properties to store the unit's level and talent points

	UPROPERTY(ReplicatedUsing = OnRep_LevelData, EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	FLevelData LevelData;

	UFUNCTION()
	void OnRep_LevelData(const FLevelData& OldLevelData);

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SetLevelData(
	int32 Experience,
	int32 CharacterLevel,
	int32 TalentPoints,
	int32 UsedTalentPoints,
	int32 AbilityPoints,
	int32 UsedAbilityPoints)
	{
		LevelData.Experience = Experience;
		LevelData.CharacterLevel = CharacterLevel;
		LevelData.TalentPoints = TalentPoints;
		LevelData.UsedTalentPoints = UsedTalentPoints;
		LevelData.AbilityPoints = AbilityPoints;
		LevelData.UsedAbilityPoints = UsedAbilityPoints;
	}
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	FLevelUpData LevelUpData;

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	int GetCharacterLevel() const
	{
		return LevelData.CharacterLevel;
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	float RegenerationTimer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	float RegenerationDelayTime = 1.f;

	// Gameplay Effects for talent point investment
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> StaminaInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> AttackPowerInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> WillpowerInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> HasteInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> ArmorInvestmentEffect;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TSubclassOf<UGameplayEffect> MagicResistanceInvestmentEffect;

	// Array of Custom Effects
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Leveling")
	TArray<TSubclassOf<UGameplayEffect>> CustomEffects;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void OnLevelUp(int NewLevel);
	// Methods for handling leveling up and investing talent points
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Leveling")
	void LevelUp();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void AutoLevelUp();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leveling")
	TArray<int32> AutolevelConfig = {1, 1, 1, 1, 1, 0};
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SetLevel(int32 CharLevel);
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoStamina();
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoAttackPower();
	
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoWillPower();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoHaste();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoArmor();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void InvestPointIntoMagicResistance();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ResetTalents();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ResetLevel();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void SaveLevelDataAndAttributes(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void LoadLevelDataAndAttributes(const FString& SlotName);
//protected:
	// Helper method to handle the actual attribute increase when a point is invested
	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void ApplyInvestmentEffect(const TSubclassOf<UGameplayEffect>& InvestmentEffect);

	// ---------------------------------------------------------------------
	//  Radial Attribute Tree (alternative to the TalentChooser)
	//  Spends the unit's LevelData.TalentPoints into attributes via a table.
	// ---------------------------------------------------------------------

	/** DataTable of FAttributeTreeNodeRow describing the ring-shaped tree. Set in the editor (present on server & client). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree")
	UDataTable* AttributeTreeDataTable = nullptr;

	/** Per-node invested points (replicated). Only nodes with >0 points are stored. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute Tree")
	TArray<FAttributeTreeNodeState> AttributeTreeNodes;

	/** Points invested into a specific node (0 if none). */
	UFUNCTION(BlueprintPure, Category = "Attribute Tree")
	int32 GetAttributeTreeNodePoints(FName NodeId) const;

	/** A node is unlocked when it is a root, or its parent (PrevId) is fully invested. */
	UFUNCTION(BlueprintPure, Category = "Attribute Tree")
	bool IsAttributeTreeNodeUnlocked(FName NodeId) const;

	/** True when the node is unlocked, not yet full, and the unit still has a talent point. */
	UFUNCTION(BlueprintPure, Category = "Attribute Tree")
	bool CanInvestInAttributeTreeNode(FName NodeId) const;

	/** Look up a node row by Id (row name). Returns nullptr if missing. */
	const FAttributeTreeNodeRow* FindAttributeTreeRow(FName NodeId) const;

	/** True if the node applies to this unit: its UnitTag is empty, or matches the unit's TalentTag / UnitTags. */
	UFUNCTION(BlueprintPure, Category = "Attribute Tree")
	bool DoesAttributeTreeNodeMatchUnit(const FAttributeTreeNodeRow& Row) const;

	/** Server-authoritative: invest one talent point through a node. Returns true if a point was actually spent. */
	UFUNCTION(BlueprintCallable, Category = "Attribute Tree")
	bool InvestInAttributeTreeNode(FName NodeId);

	/** Server-authoritative: clear the tree's node state and refund/zero all talents (via ResetTalents). */
	UFUNCTION(BlueprintCallable, Category = "Attribute Tree")
	void ResetAttributeTree();

protected:
	/** Dispatches to the matching InvestPointInto* function. Returns false only for an invalid enum. */
	bool ApplyAttributeTreeStat(EAttributeTreeStat Stat);

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leveling")
	bool OpenHealthWidget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leveling")
	bool bShowLevelOnly = false;

	UPROPERTY(BlueprintReadOnly, Category = "Leveling")
	FString CachedLevelString;

	void UpdateCachedLevelString();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leveling")
	float HealthWidgetDisplayDuration = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	void LevelVisibilityCheck();

	UFUNCTION(BlueprintCallable, Category = "Leveling")
	virtual bool UpdateLevelUpTimestamp() { return false; }
};
