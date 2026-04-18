// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "EffectAreaDuplicateProcessor.generated.h"

/**
 * Processor that handles the duplication of EffectAreas based on a timer and enemy proximity.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassEffectAreaDuplicateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UMassEffectAreaDuplicateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

	FMassEntityQuery AreaQuery;
	FMassEntityQuery EnemyQuery;
	FMassEntityQuery GlobalAreaQuery;

	float ExecutionInterval = 3.f;
	float ProcessingTime = 0.f;
	int32 NextDuplicationId = 1000;
};
