// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/UnitData.h"
#include "Characters/Unit/LevelUnit.h"
#include "StartLoadoutTypes.generated.h"

class AUnitBase;
class UGameplayEffect;
class UTexture2D;
class UMaterialInterface;
class UAutoAbility;

/**
 * A difficulty level, chosen by the (first) player before the start selection. It weakens every unit of the teams
 * without a human player as it appears - spawn tables, boss waves, the AI player's production, units already there.
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutDifficulty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (MultiLine = true))
	FText Description;

	/** Health, MaxHealth and BaseHealth of the enemies are multiplied by this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (ClampMin = "0.05"))
	float EnemyHealthMultiplier = 1.f;

	/** AttackDamage and BaseAttackDamage of the enemies are multiplied by this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (ClampMin = "0.05"))
	float EnemyDamageMultiplier = 1.f;

	/** Optional: applied to every enemy as well (after the multipliers), for anything else the level changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TSubclassOf<UGameplayEffect> EnemyEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FLinearColor AccentColor = FLinearColor(0.2f, 0.75f, 1.f, 1.f);

	/** Counts for the survival best times: only a level that leaves the enemies at full strength. */
	bool IsRanked() const { return EnemyHealthMultiplier >= 1.f && EnemyDamageMultiplier >= 1.f && !EnemyEffect; }
};

/** What a card of a start loadout stands for. */
UENUM(BlueprintType)
enum class EStartLoadoutCardType : uint8
{
	MainBase	UMETA(DisplayName = "Main Base"),
	Workers		UMETA(DisplayName = "Workers"),
	Army		UMETA(DisplayName = "Army"),
	Tech		UMETA(DisplayName = "Tech Building"),
	Production	UMETA(DisplayName = "Production Building"),
	Effect		UMETA(DisplayName = "Effect"),
	Support		UMETA(DisplayName = "Support Building"),
	AutoAbility	UMETA(DisplayName = "Auto Ability"),
	UnitType	UMETA(DisplayName = "Unit Type"),
};

/**
 * An auto ability card (UStartLoadoutComponent::AutoAbilityCardSets): switches an automatic ability on for every
 * unit of the team with UnitTag - the ones on the field and every one built later (AUpgradeGameState::
 * TeamAutoAbilityGrants). The same rows are the pool units draw from at their level milestones.
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutAutoAbility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TSubclassOf<UAutoAbility> Ability;

	/** The unit type, e.g. Units.Tier.1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FGameplayTag UnitTag;

	/** Shown on the card ("Infantry"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText TargetName;

	/** Picture of the unit type on the card (a typical unit's icon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UTexture2D> TargetIcon = nullptr;

	/** Ability and unit type given (defined in StartLoadoutComponent.cpp - UAutoAbility is only forward declared here). */
	bool IsSet() const;
};

/** One card: a unit or building class and how many of it the player starts with. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutCard
{
	GENERATED_BODY()

	/** Shown on the card. Empty = the class name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText Title;

	/** Short line under the title (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText Description;

	/** The unit or building that is spawned. Empty = the card is left out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TSubclassOf<AUnitBase> UnitClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (ClampMin = "0", ClampMax = "50"))
	int32 Count = 1;

	/** Picture on the card. Empty = the UnitIcon of UnitClass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** State the spawned units start in (workers: GoToResourceExtraction). None = the class default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TEnumAsByte<UnitData::EState> State = UnitData::None;
};

/** Talent points in one attribute, given through the unit's own investment effect (like the attribute tree). */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutStatPoints
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	EAttributeTreeStat Stat = EAttributeTreeStat::Stamina;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (ClampMin = "1", ClampMax = "50"))
	int32 Points = 1;
};

/**
 * A start bonus: every unit of the chosen set (workers and army) gets it once at spawn. All parts add up; the
 * percentages are of the unit's own value after its levels and points.
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutBonus
{
	GENERATED_BODY()

	/** Shown on the set ("+15 Vitality"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText Title;

	/** Talent points through the unit's investment effects (StaminaInvestmentEffect ...), up to MaxTalentsPerStat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TArray<FStartLoadoutStatPoints> StatPoints;

	/** Levels up, with everything a level-up brings (its talent points invested like AutoLevelUp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (ClampMin = "0", ClampMax = "20"))
	int32 Levels = 0;

	/** Health, BaseHealth and MaxHealth + this % of MaxHealth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float HealthPercent = 0.f;

	/** Shield and MaxShield + this % of MaxHealth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float ShieldPercent = 0.f;

	/** HealthRegeneration + this % of MaxHealth (per regeneration tick, one second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float HealthRegenPercent = 0.f;

	/** ShieldRegeneration + this % of MaxShield (per second; use it with ShieldPercent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float ShieldRegenPercent = 0.f;

	/** AttackDamage and BaseAttackDamage + this %. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float DamagePercent = 0.f;

	/** RunSpeed and BaseRunSpeed + this %. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float SpeedPercent = 0.f;

	/** Range + this %. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	float RangePercent = 0.f;

	bool IsEmpty() const
	{
		return StatPoints.Num() == 0 && Levels == 0 && HealthPercent == 0.f && ShieldPercent == 0.f && HealthRegenPercent == 0.f
			&& ShieldRegenPercent == 0.f && DamagePercent == 0.f && SpeedPercent == 0.f && RangePercent == 0.f;
	}
};

/** Row of the start bonus table (UStartLoadoutComponent::Bonuses): the offered sets draw their bonus from it. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutBonusRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutBonus Bonus;

	/** How often it is drawn compared to the others. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (ClampMin = "0"))
	float Weight = 1.f;
};

/**
 * Row of the start loadout table (UStartLoadoutComponent::CardSets): one card set of a faction - the player picks
 * one of the sets of his faction at match start. The main base is not part of the set, every player has it
 * (FStartLoadoutFaction::MainBase).
 */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutSetRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The faction this set belongs to (FStartLoadoutFaction::Faction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FName Faction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout", meta = (MultiLine = "true"))
	FText Description;

	/** Background of the set and its cards (a UI material of the faction). Empty = the faction's CardMaterial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UMaterialInterface> CardMaterial = nullptr;

	/** Colour of the frame when the set is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FLinearColor AccentColor = FLinearColor(0.25f, 0.8f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutCard Workers;

	/** Soldiers, robots or ships. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutCard Army;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutCard Tech;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutCard Production;

	/** More unit cards, placed beside the army - e.g. soldiers that can shoot at air units, or the passengers of a transport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TArray<FStartLoadoutCard> ExtraArmy;

	/** A building the others need to work (e.g. the reactor that powers the production building). Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutCard Support;

	/** Resets every running ability cooldown of the team's units and buildings when chosen (a card of its own if it has no army). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	bool bResetCooldowns = false;

	/** A fixed start bonus of this set. Empty = a random one from UStartLoadoutComponent::Bonuses, drawn per offer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutBonus Bonus;

	/** Auto ability card (rows of UStartLoadoutComponent::AutoAbilityCardSets): no units, no bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutAutoAbility AutoAbility;

	const FStartLoadoutCard& GetCard(EStartLoadoutCardType Type) const
	{
		switch (Type)
		{
		case EStartLoadoutCardType::Workers: return Workers;
		case EStartLoadoutCardType::Army: return Army;
		case EStartLoadoutCardType::Tech: return Tech;
		case EStartLoadoutCardType::Support: return Support;
		default: return Production;
		}
	}
};

/** A faction of the start loadout: which teams play it and the main base every one of its players has. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FStartLoadoutFaction
{
	GENERATED_BODY()

	/** Name the rows of the table refer to (FStartLoadoutSetRow::Faction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FName Faction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FText DisplayName;

	/** Teams that play this faction. Empty = a team plays it when its main base in the level is a MainBase.UnitClass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TArray<int32> TeamIds;

	/** Always there and not changeable: shown as a fixed card, spawned only if the team has no main base yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	FStartLoadoutCard MainBase;

	/** Card background when a row has none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UMaterialInterface> CardMaterial = nullptr;

	/** Picture of the "All cooldowns reset" card (FStartLoadoutSetRow::bResetCooldowns). Empty = no picture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|StartLoadout")
	TObjectPtr<UTexture2D> ResetCooldownsIcon = nullptr;
};
