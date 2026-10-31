// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/UnitData.h"
#include "TroopSequenceDirector.generated.h"

class AUnitBase;
class UGameplayEffect;
class ACameraBase;

/** Where the troops of one team appear and where they run to. */
USTRUCT(BlueprintType)
struct FTroopTeamSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	int32 TeamId = 1;

	/** The troop appears around this actor (e.g. a TargetPoint or the team's PlayerStart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	TObjectPtr<AActor> SpawnPoint = nullptr;

	/** Tag of the AWaypoint the troop runs to (AWaypoint::Tag). Empty = stays where it spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	FString WaypointTag;
};

/** One troop: Count units of UnitClass for TeamId. */
USTRUCT(BlueprintType)
struct FTroopSequenceStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	TSubclassOf<AUnitBase> UnitClass;

	/** Units of this troop. With bAdaptiveCount the director may add more, see there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "1"))
	int32 Count = 3;

	/**
	 * How well this unit fights compared to its raw sqrt(Health x AttackDamage), for bAdaptiveCount.
	 * Below 1 for units that lose more than their numbers say (e.g. melee against ranged).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0.05"))
	float PowerFactor = 1.f;

	/** Must match a TeamSetups entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	int32 TeamId = 1;

	/** Optional attribute effect for this troop (like FUnitSpawnParameter::Attributes), e.g. to make it win. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	TSubclassOf<UGameplayEffect> Attributes;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTroopSequenceStepEvent, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTroopSequenceFinishedEvent);

/**
 * Showcase battles: spawns Steps one after the other. The next troop only comes when the fight is over,
 * i.e. when one side of the units this actor spawned has nobody left alive. The new troop runs to the
 * waypoint of its team and fights the survivors there. Optionally the local camera follows the newest
 * troop. Spawns through ARTSGameModeBase::SpawnSingleUnit, so the units are registered like table spawns;
 * the GameMode's own spawn tables are not needed (leave them empty on such a map).
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ATroopSequenceDirector : public AActor
{
	GENERATED_BODY()

public:
	ATroopSequenceDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	TArray<FTroopTeamSetup> TeamSetups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	TArray<FTroopSequenceStep> Steps;

	/** Seconds after BeginPlay before the first troop (pathfinding needs a moment). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0"))
	float StartDelay = 3.f;

	/** Head start of the first troop before the second one comes (there is no fight to wait for yet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0"))
	float FirstTroopLead = 6.f;

	/** Pause between the end of a fight and the next troop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0"))
	float NextStepDelay = 2.f;

	/** Start again with Steps[0] after the last fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	bool bLoop = false;

	/**
	 * Make the new troop win: after Count units, spawn more until the troop's power is AdaptiveRatio times
	 * the power of the enemies still alive (power = sum of sqrt(Health x AttackDamage) x PowerFactor,
	 * with the survivors' current health). Never more than MaxCount.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	bool bAdaptiveCount = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "1", EditCondition = "bAdaptiveCount"))
	float AdaptiveRatio = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "1", EditCondition = "bAdaptiveCount"))
	int32 MaxCount = 30;

	/**
	 * If the new troop loses anyway, send the same step again (counting the survivors, so it comes
	 * bigger) up to this many times before the sequence goes on. 0 = never.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0"))
	int32 MaxReinforcements = 3;

	/**
	 * Units of both sides alive, but nobody takes damage for this long (stuck, out of reach, e.g. melee
	 * against fliers): the weaker side falls, so the sequence goes on. 0 = wait forever.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0"))
	float StalemateTimeout = 20.f;

	/** Units spread randomly within this radius around the spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0"))
	float SpawnRadius = 300.f;

	/** Height of the spawned units (SpawnSingleUnit takes Z from the offset, not from the location). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	float SpawnHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	TEnumAsByte<UnitData::EState> SpawnState = UnitData::PatrolRandom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	bool bUnitsSelectable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence", meta = (ClampMin = "0.05"))
	float CheckInterval = 0.5f;

	/** Turns the Fog of War off on this machine (ACustomControllerBase::SetFogOfWarCheatDisabled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	bool bDisableFogOfWar = true;

	/**
	 * The local player may select the units of every team (SelectableTeamId 0), e.g. to look at them in the
	 * selector's 3D window. Without it the selection only takes the player's own team.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	bool bSelectAllTeams = true;

	/** Each new troop is selected for the local player, so the UI (unit selector, 3D window) shows it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence")
	bool bSelectNewTroop = true;

	/** The local camera follows the newest troop (its alive units' centre; the survivors once it is gone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence|Camera")
	bool bFollowNewTroop = true;

	/** Spring arm length while following. 0 = leave the zoom alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence|Camera", meta = (ClampMin = "0", EditCondition = "bFollowNewTroop"))
	float FollowArmLength = 1800.f;

	/** How fast the camera catches up (interp speed, higher = tighter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TroopSequence|Camera", meta = (ClampMin = "0.1", EditCondition = "bFollowNewTroop"))
	float FollowSpeed = 2.5f;

	UFUNCTION(BlueprintCallable, Category = "TroopSequence")
	void StartSequence();

	UFUNCTION(BlueprintCallable, Category = "TroopSequence")
	void StopSequence();

	UFUNCTION(BlueprintPure, Category = "TroopSequence")
	int32 GetCurrentStepIndex() const { return CurrentStep; }

	UPROPERTY(BlueprintAssignable, Category = "TroopSequence")
	FTroopSequenceStepEvent OnStepSpawned;

	/** A fight is over; StepIndex = the troop that was spawned last. */
	UPROPERTY(BlueprintAssignable, Category = "TroopSequence")
	FTroopSequenceStepEvent OnFightOver;

	UPROPERTY(BlueprintAssignable, Category = "TroopSequence")
	FTroopSequenceFinishedEvent OnSequenceFinished;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void SpawnStep(int32 Index);
	bool IsFightOver() const;
	void UpdateCamera(float DeltaSeconds);
	const FTroopTeamSetup* FindTeamSetup(int32 TeamId) const;
	static bool IsAlive(const AUnitBase* Unit);
	/** Centre of the alive units in Units; false if none. */
	static bool GetAliveCentre(const TArray<TWeakObjectPtr<AUnitBase>>& Units, FVector& OutCentre);
	ACameraBase* GetLocalCamera() const;
	void ApplyFogOfWar();

	/** Replaces the local player's selection with the alive units of CurrentTroop. */
	void SelectCurrentTroop();
	/** Sum of Health + Shield of the alive spawned units. */
	float GetTotalHealth() const;
	void ResolveStalemate();
	/** sqrt(Health x AttackDamage) x the PowerFactor of the unit's step. */
	float GetUnitPower(const AUnitBase* Unit) const;

	/** Units of the newest troop. */
	TArray<TWeakObjectPtr<AUnitBase>> CurrentTroop;

	/** Every unit this actor spawned that may still be alive. */
	TArray<TWeakObjectPtr<AUnitBase>> AllSpawned;

	/** PowerFactor of the step each unit came with. */
	TMap<TObjectKey<AUnitBase>, float> UnitPowerFactors;

	int32 CurrentStep = INDEX_NONE;
	bool bRunning = false;
	bool bWaitingForNext = false;
	float StartTimer = -1.f;
	float NextTimer = 0.f;
	float CheckTimer = 0.f;
	bool bFogApplied = false;
	/** The next spawn repeats CurrentStep (the new troop lost). */
	bool bRepeatStep = false;
	int32 Reinforcements = 0;
	float StalemateTime = 0.f;
	float LastTotalHealth = -1.f;
};
