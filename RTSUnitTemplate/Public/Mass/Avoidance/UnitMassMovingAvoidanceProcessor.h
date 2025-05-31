// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Avoidance/MassAvoidanceProcessors.h" // Include engine header
#include "MassExecutionContext.h"             // Include for group names
#include "UnitMassMovingAvoidanceProcessor.generated.h"

/**
 * Custom version of UMassMovingAvoidanceProcessor for debugging.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitMassMovingAvoidanceProcessor : public UMassMovingAvoidanceProcessor
{
	GENERATED_BODY()

public:
	UUnitMassMovingAvoidanceProcessor();

protected:
	virtual void ConfigureQueries() override;
	// We might not strictly need to override ConfigureQueries if the base class's is sufficient,
	// but it's here if you need to adjust requirements later.
	// virtual void ConfigureQueries() override;

	// Override Execute to add logging around the base class execution.
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	// Separate query JUST for logging purposes
	FMassEntityQuery DebugLogQuery; // <<< Add member variable
};