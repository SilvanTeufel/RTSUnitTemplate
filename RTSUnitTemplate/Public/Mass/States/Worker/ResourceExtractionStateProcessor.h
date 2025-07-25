// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"             // Required for FMassEntityQuery
#include "MassSignalSubsystem.h"
#include "ResourceExtractionStateProcessor.generated.h"

// Forward declaration
struct FMassExecutionContext;

/**
 * Processor handling the logic for units actively extracting resources.
 * It manages the extraction timer and signals completion.
 */
UCLASS()
class RTSUNITTEMPLATE_API UResourceExtractionStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UResourceExtractionStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};