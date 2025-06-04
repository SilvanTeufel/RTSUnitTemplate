// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ChargeMonitorProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UChargeMonitorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UChargeMonitorProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;
};
