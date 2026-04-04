// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "RunAnimationProcessor.generated.h"

/**
 * Processor that handles the FRunAnimationTag and its associated timer.
 */
UCLASS()
class RTSUNITTEMPLATE_API URunAnimationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	URunAnimationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
