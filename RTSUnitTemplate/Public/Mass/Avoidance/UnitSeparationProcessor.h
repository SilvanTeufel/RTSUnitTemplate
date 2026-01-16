// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassNavigationFragments.h"
#include "UnitSeparationProcessor.generated.h"

struct FMassExecutionContext;

	/**
	 * Applies a lateral repulsion force between nearby units to avoid clumping.
	 * Includes all active states (Attack, Run, Chase, Pause, Build, Repair).
	 * Uses FMassForceFragment so the existing movement processor can consume it.
	 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitSeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UUnitSeparationProcessor();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = true;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	// Throttling
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExecutionInterval = 0.1f;
	
	// Separate repulsion strengths for friendly vs enemy interactions
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float RepulsionStrengthFriendly = 50.f;

	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float RepulsionStrengthEnemy = 30.f;

	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float RepulsionStrengthWorker = 5.f;

	// If true, only apply separation to units that share the same target entity.
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	bool bOnlySeparateWhenSameTarget = false;

	// Optional cap to limit checks radius (performance). 0 disables extra culling.
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float MaxCheckRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float DistanceMultiplierFriendly = 1.2f;
	
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float DistanceMultiplierEnemy = 1.0f;
private:
	FMassEntityQuery EntityQuery;
	
	float TimeSinceLastRun = 0.f;
};
