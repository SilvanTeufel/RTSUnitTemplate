// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassProcessor.h"
// Declares FNavigationObstacleHashGrid2D (typedef of THierarchicalHashGrid2D) and its FCell/FItem.
#include "MassNavigationSubsystem.h"
#include "UnitObstacleSnapshotSubsystem.generated.h"

/**
 * Immutable per-frame copy of UMassNavigationSubsystem's avoidance obstacle grid.
 *
 * Why this exists: UUnitMovingAvoidanceProcessor wants to run off the game thread, but the
 * engine grid is a TSet<FCell> + TSparseArray<FItem> that keeps being mutated while we read
 * it - UMassNavigationObstacleRemoverProcessor is a UMassObserverProcessor, so it fires on
 * entity destruction outside the phase dependency graph and no ExecuteAfter can order us
 * against it. Reading it concurrently crashed in TSparseArray::IsValidIndex(), which indexes
 * the allocation bit array and asserts ("Index>=0 && Index<NumBits") instead of returning
 * false, so the defensive check in FindCloseObstacles could never help.
 *
 * UUnitObstacleSnapshotProcessor rebuilds this copy once per frame ON THE GAME THREAD, where
 * it cannot overlap with those mutations. Everything that reads it afterwards is read-only and
 * therefore safe from any thread.
 *
 * Only the mutable parts are copied. Cell size / level configuration is set up once at grid
 * construction and never changes, so the avoidance code keeps reading that straight off the
 * live grid for the query-bounds math.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitObstacleSnapshotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Copy the grid's mutable state. Game thread only. */
	void Rebuild(const FNavigationObstacleHashGrid2D& Grid);

	/** Cell lookup against the snapshot, mirroring FHierarchicalHashGrid2D::FindCell. */
	FORCEINLINE const FNavigationObstacleHashGrid2D::FCell* FindCell(const int32 X, const int32 Y, const int32 Level) const
	{
		return Cells.Find(FNavigationObstacleHashGrid2D::FCell(X, Y, Level));
	}

	FORCEINLINE const TSparseArray<FNavigationObstacleHashGrid2D::FItem>& GetItems() const { return Items; }

	/** False until the first Rebuild; callers must fall back to doing nothing, never to the live grid. */
	FORCEINLINE bool IsValidSnapshot() const { return bBuilt; }

private:
	TSet<FNavigationObstacleHashGrid2D::FCell> Cells;
	TSparseArray<FNavigationObstacleHashGrid2D::FItem> Items;
	bool bBuilt = false;
};

/**
 * Rebuilds UUnitObstacleSnapshotSubsystem once per frame, on the game thread, after the engine
 * has finished updating the obstacle grid and before the Avoidance group consumes it.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitObstacleSnapshotProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitObstacleSnapshotProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
