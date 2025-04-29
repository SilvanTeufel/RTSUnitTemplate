// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"         // Required for FMassEntityQuery
#include "GoToResourceExtractionStateProcessor.generated.h"

// Forward declaration
struct FMassExecutionContext;

/**
 * Processor for units moving towards a resource node to start extraction.
 * Checks for arrival and signals when the unit is close enough.
 */
UCLASS()
class RTSUNITTEMPLATE_API UGoToResourceExtractionStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGoToResourceExtractionStateProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	// Helper function pointer if needed, or include the header where UpdateMoveTarget/StopMovement are defined
	// (Assuming they are globally accessible or part of a utility class)
};