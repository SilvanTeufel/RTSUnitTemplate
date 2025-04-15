// Fill out your copyright notice in the Description page of Project Settings.
/*
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

	
*/
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"     // FTransformFragment
#include "MassMovementFragments.h"  // FMassVelocityFragment, FMassMoveTargetFragment
#include "MassNavigationFragments.h" // FUnitNavigationPathFragment (Assumes this exists from previous step)
#include "UnitNavigationFragments.h"
#include "UnitMovementProcessor.generated.h"

// Forward Declarations
class UNavigationSystemV1;
class UWorld;
struct FMassExecutionContext;

UCLASS()
class RTSUNITTEMPLATE_API UUnitMovementProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UUnitMovementProcessor();

protected:
    // Configuration function called during initialization.
    virtual void ConfigureQueries() override;

    // Execute is called during the processing phase and applies the logic on each entity chunk.
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float AccumulatedTime = 0.0f;
private:

    // Query to select entities with the required movement and navigation components.
    FMassEntityQuery EntityQuery;
};
