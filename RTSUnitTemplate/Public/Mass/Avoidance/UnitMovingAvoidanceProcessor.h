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
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	// These members were private in the original, so we must re-declare them here.
	TObjectPtr<UWorld> World;
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};
