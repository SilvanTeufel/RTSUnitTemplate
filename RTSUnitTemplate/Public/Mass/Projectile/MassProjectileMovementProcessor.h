#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassProjectileMovementProcessor.generated.h"

/**
 * Processor for projectile movement.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassProjectileMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassProjectileMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
