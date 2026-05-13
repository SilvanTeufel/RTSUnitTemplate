// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassEntityTypes.h"
#include "MassEffectAreaHoverProcessor.generated.h"

/**
 * Processor for debugging EffectAreas on the client.
 * Logs visibility and scaling information when an EffectArea is hovered.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassEffectAreaHoverProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassEffectAreaHoverProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY()
	FMassEntityHandle LastHoveredEntity;

	FMassEntityQuery EntityQuery;
	float AccumulatedTime = 0.f;
};
