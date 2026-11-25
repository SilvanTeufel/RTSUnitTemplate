// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GAS/AutoAbility.h"
#include "AutoAbilitySubsystem.generated.h"

class AUnitBase;
class AUpgradeGameState;

/**
 * Server: which automatic abilities (UAutoAbility) every unit has, their cooldowns, and the events that set them off.
 *
 * Two sources:
 *  - the team (AUpgradeGameState::TeamAutoAbilityGrants): an auto ability card or a tree node switches an ability
 *    on for every unit of one type (Units.Tier.N) - the ones on the field and every one built later (synced once
 *    a second);
 *  - the unit itself: at every LevelUpData.AbilityPointsEveryXLevel levels (5, 10, 15, 20 ...) it gets one more,
 *    drawn at random from the auto abilities its type can have (the auto ability cards of its faction).
 *
 * The game calls NotifyTrigger from the attack / hit / engage events (UUnitStateProcessor, AProjectile); Periodic
 * abilities are checked once a second here.
 */
UCLASS()
class RTSUNITTEMPLATE_API UAutoAbilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Server: an event of Unit (Other = the attacked unit / the attacker). Cheap when the unit has nothing. */
	static void NotifyTrigger(AUnitBase* Unit, EAutoAbilityTrigger Trigger, AUnitBase* Other = nullptr);

	/** Server: the unit reached LevelMilestones x AbilityPointsEveryXLevel levels - draws the missing level abilities.
	 *  True when the new system handled it (false = no pool for this unit, the old ability slots apply). */
	bool SyncLevelAbilities(AUnitBase* Unit, int32 Milestones);

	/** The abilities a unit has right now (server) - for tests and tooltips. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|AutoAbility")
	TArray<TSubclassOf<UAutoAbility>> GetUnitAutoAbilities(AUnitBase* Unit) const;

	/** Seconds until the ability of the unit is ready again (0 = ready, -1 = the unit does not have it). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|AutoAbility")
	float GetCooldownRemaining(AUnitBase* Unit, TSubclassOf<UAutoAbility> Ability) const;

	/** Test: sets the ability off now (cooldown ignored and restarted). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|AutoAbility")
	bool ForceTrigger(AUnitBase* Unit, TSubclassOf<UAutoAbility> Ability, AUnitBase* Other = nullptr);

	/** The auto abilities units with this tag can have in the faction of TeamId (from the auto ability cards). */
	void GetPoolForUnit(const AUnitBase* Unit, TArray<TSubclassOf<UAutoAbility>>& OutPool) const;

	/** Runs the once-a-second sync now (tests). */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|AutoAbility")
	void SyncNow() { TickSync(); }

private:
	struct FAbilityState
	{
		TSubclassOf<UAutoAbility> Ability;
		float CooldownMultiplier = 1.f;
		double ReadyAt = 0.0;
		bool bFromTeam = false;
		bool bFromLevel = false;
	};

	struct FUnitEntry
	{
		TArray<FAbilityState> Abilities;
		int32 TeamVersion = -1;
		int32 LevelAbilities = 0;
	};

	void TickSync();
	/** Copies the entry into the unit's replicated AutoAbilityStatus (only when it changed). */
	void PublishStatus(AUnitBase* Unit, const FUnitEntry& Entry) const;
	void RebuildFromTeam(AUnitBase* Unit, FUnitEntry& Entry, const AUpgradeGameState* GameState) const;
	bool TryFire(AUnitBase* Unit, FUnitEntry& Entry, FAbilityState& State, AUnitBase* Other, bool bIgnoreCooldown = false);
	AUpgradeGameState* GetGameState() const;

	TMap<TWeakObjectPtr<AUnitBase>, FUnitEntry> Units;
	FTimerHandle SyncTimer;
};
