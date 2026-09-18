// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Avoidance/UnitObstacleSnapshotSubsystem.h"

#include "MassExecutionContext.h"
#include "MassNavigationSubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"

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

void UUnitObstacleSnapshotSubsystem::ResetAgents()
{
	check(IsInGameThread());
	// Nur die Belegtmarke loeschen statt das Feld zu verkleinern: die Groesse pendelt sich nach
	// wenigen Bildern auf den Hoechststand ein, danach faellt keine Speicheranforderung mehr an.
	for (FUnitObstacleAgentSnapshot& Entry : Agents)
	{
		Entry.bOccupied = false;
	}
	bAgentsBuilt = true;
}

void UUnitObstacleSnapshotSubsystem::AddAgent(const FMassEntityHandle Entity, const FUnitObstacleAgentSnapshot& Data)
{
	check(IsInGameThread());
	if (Entity.Index < 0)
	{
		return;
	}
	if (!Agents.IsValidIndex(Entity.Index))
	{
		Agents.SetNum(Entity.Index + 1, EAllowShrinking::No);
	}
	FUnitObstacleAgentSnapshot& Target = Agents[Entity.Index];
	Target = Data;
	Target.SerialNumber = Entity.SerialNumber;
	Target.bOccupied = true;
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
	ProcessorRequirements.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<UUnitObstacleSnapshotSubsystem>(EMassFragmentAccess::ReadWrite);

	// Nachbardaten einsammeln. Lage und Radius sind Pflicht - der Ausweichcode las beide frueher
	// ohne Absicherung (GetFragmentDataChecked / GetFragmentData), setzte ihr Vorhandensein also
	// ohnehin voraus. Die uebrigen drei sind optional, weil der alte Code sie ueber
	// GetFragmentDataPtr holte und auf nullptr geprueft hat.
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitObstacleSnapshotProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UUnitObstacleSnapshotProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UUnitObstacleSnapshotProcessor);

	const UMassNavigationSubsystem* NavSubsystem = Context.GetSubsystem<UMassNavigationSubsystem>();
	UUnitObstacleSnapshotSubsystem* Snapshot = Context.GetMutableSubsystem<UUnitObstacleSnapshotSubsystem>();
	if (!NavSubsystem || !Snapshot)
	{
		return;
	}

	Snapshot->Rebuild(NavSubsystem->GetObstacleGrid());

	// Zweiter Teil: die Fragmentdaten der Nachbarn. Siehe FUnitObstacleAgentSnapshot - ohne sie
	// muesste UUnitMovingAvoidanceProcessor weiterhin lebende EntityViews ueber FREMDE Entitaeten
	// bauen und koennte deshalb nicht parallel laufen.
	Snapshot->ResetAgents();
	EntityQuery.ForEachEntityChunk(Context, [Snapshot](FMassExecutionContext& ChunkContext)
	{
		const int32 Count = ChunkContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = ChunkContext.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FMassAvoidanceColliderFragment> ColliderList = ChunkContext.GetFragmentView<FMassAvoidanceColliderFragment>();

		// Optionale Fragmente liefern eine LEERE Sicht, wenn der Archetyp sie nicht hat.
		const bool bHasVelocity = VelocityList.Num() > 0;
		const bool bHasMoveTarget = MoveTargetList.Num() > 0;
		const bool bHasColliderFragment = ColliderList.Num() > 0;

		for (int32 i = 0; i < Count; ++i)
		{
			FUnitObstacleAgentSnapshot Data;

			const FTransform& Xf = TransformList[i].GetTransform();
			Data.Location = Xf.GetLocation();
			Data.Forward = Xf.GetRotation().GetForwardVector();
			Data.AgentRadius = RadiusList[i].Radius;
			Data.Velocity = bHasVelocity ? VelocityList[i].Value : FVector::ZeroVector;

			// Genau die Bedeutung aus dem alten Code: bCanAvoid == "hat ueberhaupt ein
			// Bewegungsziel", und ohne Bewegungsziel gilt der Nachbar als beweglich.
			Data.bCanAvoid = bHasMoveTarget;
			Data.bIsMoving = bHasMoveTarget
				? (MoveTargetList[i].GetCurrentAction() == EMassMovementAction::Move)
				: true;

			if (bHasColliderFragment)
			{
				Data.bHasCollider = true;
				if (ColliderList[i].Type == EMassColliderType::Circle)
				{
					Data.ColliderType = 0;
					Data.ColliderRadius = ColliderList[i].GetCircleCollider().Radius;
				}
				else
				{
					const FMassPillCollider Pill = ColliderList[i].GetPillCollider();
					Data.ColliderType = 1;
					Data.ColliderRadius = Pill.Radius;
					Data.PillHalfLength = Pill.HalfLength;
				}
			}

			Snapshot->AddAgent(ChunkContext.GetEntity(i), Data);
		}
	});
}
