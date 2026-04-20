#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassBeaconProcessor.generated.h"

/**
 * Processor that updates the URTSBeaconSubsystem with current beacon locations and ranges.
 * Throttled to a lower frequency (0.5s) as beacon locations are relatively static.
 * Targets only entities with FMassBeaconFragment.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassBeaconProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassBeaconProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	
	UPROPERTY(EditAnywhere, Category = "Mass")
	float ExecutionInterval = 0.5f;

	float TimeSinceLastRun = 0.f;
};
