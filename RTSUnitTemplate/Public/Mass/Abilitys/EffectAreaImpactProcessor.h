// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "MassProcessor.h"
#include "EffectAreaImpactProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassEffectAreaImpactProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassEffectAreaImpactProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery AreaQuery;
	FMassEntityQuery UnitQuery;
};
