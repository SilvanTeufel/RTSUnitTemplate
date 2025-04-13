// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "MassEntityTypes.h"
#include "Math/Vector.h"
#include "MassNavigationFragments.h"

#include "UnitMovementProcessor.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API UUnitMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
public:
	UUnitMovementProcessor();
	
protected:
	virtual void ConfigureQueries() override;

	// Execute is called during the processing phase and applies the logic on each entity chunk.
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	
	FMassEntityQuery EntityQuery;
};
