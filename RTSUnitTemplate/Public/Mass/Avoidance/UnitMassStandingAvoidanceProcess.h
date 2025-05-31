// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Avoidance/MassAvoidanceProcessors.h" // Include engine header
#include "MassExecutionContext.h"             // Include for group names
#include "MassEntityQuery.h"                  // Include for FMassEntityQuery
#include "UnitMassStandingAvoidanceProcess.generated.h"

// Forward declare the moving processor if needed for ExecuteAfter
class UUnitMassMovingAvoidanceProcessor;

/**
 * Custom version of UMassStandingAvoidanceProcessor for debugging.
 * Note the class name ends in 'Process' as requested by the user.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitMassStandingAvoidanceProcess : public UMassStandingAvoidanceProcessor
{
	GENERATED_BODY()

public:
	UUnitMassStandingAvoidanceProcess();

protected:
	// Override ConfigureQueries to setup our debug query
	virtual void ConfigureQueries() override;

	// Override Execute to add logging around the base class execution.
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	// Separate query JUST for logging purposes
	FMassEntityQuery DebugLogQuery;
};
