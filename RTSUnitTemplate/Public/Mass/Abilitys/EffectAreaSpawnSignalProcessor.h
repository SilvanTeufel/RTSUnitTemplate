// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "EffectAreaSpawnSignalProcessor.generated.h"

/**
 * Processor that handles the SpawnEffectAreaRequested signal.
 */
UCLASS()
class RTSUNITTEMPLATE_API UEffectAreaSpawnSignalProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UEffectAreaSpawnSignalProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

	FMassEntityQuery SpawnQuery;
};
