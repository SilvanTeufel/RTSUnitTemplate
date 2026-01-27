// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "UnitSoftAvoidanceProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UUnitSoftAvoidanceProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UUnitSoftAvoidanceProcessor();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = false;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExecutionInterval = 0.1f;
	
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float AvoidanceStrength = 1500.f;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	float ZExtent = 5000.f;
	
private:
	FMassEntityQuery EntityQuery;
	float TimeSinceLastRun = 0.f;
};
