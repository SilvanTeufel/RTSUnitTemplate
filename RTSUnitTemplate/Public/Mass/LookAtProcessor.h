// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LookAtProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ULookAtProcessor : public UMassProcessor
{
	GENERATED_BODY()

	ULookAtProcessor();
public:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float AccumulatedTimeA = 0.0f;

private:
	FMassEntityQuery EntityQuery;
};
