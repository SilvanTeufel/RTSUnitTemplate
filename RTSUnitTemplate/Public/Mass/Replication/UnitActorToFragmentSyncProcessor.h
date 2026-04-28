// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "UnitActorToFragmentSyncProcessor.generated.h"

/**
 * Processor to synchronize data from AUnitBase actors to Mass fragments locally on both Server and Client.
 * This allows reducing the network payload by not replicating data that is already replicated via the Actor system.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitActorToFragmentSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitActorToFragmentSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
