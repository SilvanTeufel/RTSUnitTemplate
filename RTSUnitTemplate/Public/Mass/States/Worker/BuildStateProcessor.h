// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassEntityTypes.h"
#include "BuildStateProcessor.generated.h"

struct FMassAIStateFragment;
struct FMassWorkerStatsFragment;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UBuildStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UBuildStateProcessor();
public:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx);

	void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};
