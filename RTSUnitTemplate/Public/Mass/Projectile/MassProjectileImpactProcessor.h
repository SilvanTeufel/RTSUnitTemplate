#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassProjectileImpactProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassProjectileImpactProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassProjectileImpactProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ProjectileQuery;
	FMassEntityQuery UnitQuery;
};
