// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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

	//ULookAtProcessor();
public:
	//virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	//virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float AccumulatedTimeA = 0.0f;

private:
	FMassEntityQuery EntityQuery;
};
