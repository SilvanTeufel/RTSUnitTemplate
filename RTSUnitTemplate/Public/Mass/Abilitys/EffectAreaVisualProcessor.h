#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "EffectAreaVisualProcessor.generated.h"

/**
 * Processor to handle visual updates for effect areas (ISM instances, Niagara, etc.)
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassEffectAreaVisualProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassEffectAreaVisualProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery VisualQuery;
	FMassEntityQuery CleanupQuery;
};
