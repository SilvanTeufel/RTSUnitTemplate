// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "AttackPauseClumpSeparationProcessor.generated.h"

struct FMassExecutionContext;

/**
 * Applies a simple repulsion force between nearby units that are currently in Attack or Pause state
 * to avoid clumping up while engaging the same target area. Uses FMassForceFragment so the existing
 * movement processor (UUnitApplyMassMovementProcessor) can consume it in the Avoidance->Movement order.
 */
UCLASS()
class RTSUNITTEMPLATE_API UAttackPauseClumpSeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UAttackPauseClumpSeparationProcessor();

	// Debug toggle
	bool bShowLogs = false;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	// Throttling
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExecutionInterval = 0.25f;

	// Strength multiplier for separation. Units: (cm/s^2) like a pseudo acceleration added once per tick.
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float RepulsionStrength = 100.f;

	// If true, only apply separation to units that share the same target entity.
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	bool bOnlySeparateWhenSameTarget = true;

	// Optional cap to limit checks radius (performance). 0 disables extra culling.
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float MaxCheckRadius = 300.f;

	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float DistanceMultiplier = 2.f;
private:
	// We keep two queries since Mass tag queries do not support OR within a single requirement.
	FMassEntityQuery AttackQuery;
	FMassEntityQuery PauseQuery;

	float TimeSinceLastRun = 0.f;
};
