// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"         // Required for FMassEntityQuery
#include "MassSignalSubsystem.h"
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
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};