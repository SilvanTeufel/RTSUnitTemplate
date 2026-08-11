// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassEntityQuery.h"
#include "PauseStateProcessor.generated.h"

class UMassSignalSubsystem;
class UMassEntitySubsystem;
struct FMassExecutionContext;
struct FMassStatePauseTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassAgentCharacteristicsFragment;
struct FMassEntityHandle;
struct FMassStateAttackTag;
struct FMassStateChaseTag;

UCLASS()
class RTSUNITTEMPLATE_API UPauseStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UPauseStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	// Handler für das Schuss-Signal
	UFUNCTION()
	void OnProjectileSignalReceived(FName SignalName, const TArray<FMassEntityHandle>& Entities);

	// Kapselung der Spawn-Logik
	void ExecuteProjectileSpawn(FMassEntityManager& EntityManager, const FMassEntityHandle Entity);

	static float GetCombinedRadii(const FMassAgentCharacteristicsFragment& AttackerChar, const FTransform& AttackerTransform, 
								  const FMassAgentCharacteristicsFragment* TargetChar, const FTransform* TargetTransform, 
								  const FVector& TargetLocation);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	/**
	 * Laesst eine bereits laufende Angriffspause zu Ende laufen, wenn das Ziel dazwischen
	 * stirbt, statt sofort abzubrechen.
	 *
	 * Der Schuss einer Projektil-Einheit faellt NICHT im Attack-Zustand, sondern am Ende
	 * dieser Pause (UnitSignals::RangedAttack). Die ResonanceCannon hat gesiegt eine
	 * PauseDuration von 3 s - sie muss also dasselbe Ziel drei Sekunden am Leben halten.
	 * Starb es vorher, brach der Zustand ab, StateTimer ging auf 0 und die Kanone begann
	 * beim naechsten Ziel von vorn: im Getuemmel kam sie damit nie zum Schuss.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool bFinishPauseOnTargetLoss = true;

	/**
	 * Hysterese fuer den Austritt aus der Pause Richtung Chase/Run - dasselbe Totband wie
	 * im AttackStateProcessor, damit eine Einheit an der Reichweitengrenze nicht zwischen
	 * den Zustaenden flackert und dabei jedes Mal ihren Angriffszyklus verliert.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate, meta = (ClampMin = "1.0"))
	float AttackRangeHysteresis = 1.15f;

private:
	void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& CombatStats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor);

	void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& CombatStats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor);

	void ApplyAttackStopLogic(FMassExecutionContext& Context, 
		const FMassCombatStatsFragment& Stats, const FMassAITargetFragment& TargetFrag, 
		const FMassEntityHandle Entity, const int32 EntityIdx);

	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;


	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;

	FDelegateHandle ProjectileSignalDelegateHandle;

	float FollowAcceptanceMultiplier = 6.f;
};
