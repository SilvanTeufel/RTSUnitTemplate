// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Avoidance/UnitObstacleSnapshotSubsystem.h"

#include "MassExecutionContext.h"
#include "MassNavigationSubsystem.h"

void UUnitObstacleSnapshotSubsystem::Rebuild(const FNavigationObstacleHashGrid2D& Grid)
{
	check(IsInGameThread());

	// FCell and FItem are plain value types, so these are straight container copies. The cost
	// scales with the number of registered obstacles, not with the number of avoidance queries,
	// and it buys back running the whole avoidance pass in parallel.
	Cells = Grid.GetCells();
	Items = Grid.GetItems();
	bBuilt = true;
}

UUnitObstacleSnapshotProcessor::UUnitObstacleSnapshotProcessor(): EntityQuery()
{
	// Same phase as the avoidance work it feeds.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;

	// Must observe the grid AFTER the engine finished updating it and BEFORE anything in the
	// Avoidance group reads the snapshot.
	ExecutionOrder.ExecuteAfter.Add(FName("MassNavigationObstacleGridProcessor"));
	ExecutionOrder.ExecuteBefore.Add(FName("Avoidance"));

	// The whole point: the copy happens where the engine also mutates the grid.
	bRequiresGameThreadExecution = true;

	ExecutionFlags = static_cast<uint8>(EProcessorExecutionFlags::Standalone
		| EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
}

void UUnitObstacleSnapshotProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	// No entities are touched; the processor only needs access to the two subsystems.
	ProcessorRequirements.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<UUnitObstacleSnapshotSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitObstacleSnapshotProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UMassNavigationSubsystem* NavSubsystem = Context.GetSubsystem<UMassNavigationSubsystem>();
	UUnitObstacleSnapshotSubsystem* Snapshot = Context.GetMutableSubsystem<UUnitObstacleSnapshotSubsystem>();
	if (!NavSubsystem || !Snapshot)
	{
		return;
	}

	Snapshot->Rebuild(NavSubsystem->GetObstacleGrid());
}
