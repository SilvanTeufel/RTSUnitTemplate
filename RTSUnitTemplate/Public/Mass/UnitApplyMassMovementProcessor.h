// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "UnitApplyMassMovementProcessor.generated.h"

// Forward declarations
struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitApplyMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UUnitApplyMassMovementProcessor();

	// Global logging toggle for this processor
	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool bShowLogs = false;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	float SoftAvoidanceZExtent = 5000.f;
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	FMassEntityQuery ClientEntityQuery;
	
	// Separated execution paths
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
};
