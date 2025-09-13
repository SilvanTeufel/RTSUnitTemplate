// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Avoidance/MassAvoidanceProcessors.h"
#include "UnitMovingAvoidanceProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitMovingAvoidanceProcessor : public UMassMovingAvoidanceProcessor
{
	GENERATED_BODY()

public:
	// Constructor
	UUnitMovingAvoidanceProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

private:
	FMassEntityQuery EntityQuery;
};
