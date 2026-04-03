// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "PauseStateProcessor.generated.h"

struct FMassExecutionContext;
struct FMassStatePauseTag; // Tag für diesen Zustand
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassVelocityFragment;
struct FMassStateAttackTag; // Zielzustand
struct FMassStateChaseTag; // Zielzustand

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
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& CombatStats, const FMassEntityHandle Entity, const int32 EntityIdx);

	void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		const FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& CombatStats, const FMassEntityHandle Entity, const int32 EntityIdx);

	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};