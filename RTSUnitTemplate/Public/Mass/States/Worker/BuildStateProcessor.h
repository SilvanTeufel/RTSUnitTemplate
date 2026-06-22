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
	virtual void BeginDestroy() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx);

	void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
		FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	// While building, face the BuildArea center. Only re-aim when the heading error exceeds this many
	// degrees (dead-zone, prevents micro-jitter). Runs on server AND client (BuildArea is replicated).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float BuildFacingToleranceDeg = 2.5f;

private:
	FMassEntityQuery EntityQuery;

	// Rotate the entity's transform to look at its (replicated) BuildArea center. No-op while within
	// BuildFacingToleranceDeg. Shared by server and client so the facing matches on both.
	void FaceBuildArea(FMassExecutionContext& Context, const int32 EntityIdx);

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UFUNCTION()
	void CalculateConstructionScale(FName SignalName, TArray<FMassEntityHandle>& Entities);

	FDelegateHandle CalculateConstructionScaleDelegateHandle;
};
