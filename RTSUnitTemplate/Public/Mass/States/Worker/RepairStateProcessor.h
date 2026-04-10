// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassEntityTypes.h"
#include "RepairStateProcessor.generated.h"

struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;

UCLASS()
class RTSUNITTEMPLATE_API URepairStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URepairStateProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& AIState, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx);

	void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& AIState, const FMassAITargetFragment& TargetFrag, 
		const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

private:
	FMassEntityQuery EntityQuery;
	float TimeSinceLastRun = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};