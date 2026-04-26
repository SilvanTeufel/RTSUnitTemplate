// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "PauseStateProcessor.generated.h"

class UMassSignalSubsystem;
class UMassEntitySubsystem;
struct FMassExecutionContext;
struct FMassStatePauseTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& CombatStats, const FMassEntityHandle Entity, const int32 EntityIdx);

	void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		const FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& CombatStats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor);

	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;

	FDelegateHandle ProjectileSignalDelegateHandle;
};