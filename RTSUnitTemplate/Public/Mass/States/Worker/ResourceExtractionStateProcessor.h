// Fill out your copyright notice in the Description page of Project Settings.

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
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};