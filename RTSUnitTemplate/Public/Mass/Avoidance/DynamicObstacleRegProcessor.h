// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "DynamicObstacleRegProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UDynamicObstacleRegProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
public:
	UDynamicObstacleRegProcessor();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
private:
	FMassEntityQuery ObstacleQuery;

	float TimeSinceLastRun = 0.0f;
};
