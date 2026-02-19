// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "Mass/Abilitys/DecalScalingFragments.h"
#include "MassDecalScalingProcessor.generated.h"

/**
 * Processor that handles smooth scaling of Area Decals using Mass entities.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassDecalScalingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassDecalScalingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(EditAnywhere, Category = "Mass")
	float ExecutionInterval = 0.1f;

private:
	FMassEntityQuery EntityQuery;
	float TimeSinceLastRun = 0.f;
};
