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
/**
 * Alles, was die Ausweichlogik von einem NACHBARN liest - als reiner Wertetyp.
 *
 * WOFUER: UUnitMovingAvoidanceProcessor las diese Angaben je Nachbar und je Bild direkt aus
 * dessen Fragmenten (FMassEntityView / GetFragmentDataChecked). Das sind Fremd-Entitaetszugriffe,
 * und genau sie sind der Grund, warum der Prozessor auf dem Spielthread festgenagelt ist: ein
 * paralleler Lauf liess den CurrentArchetype-Wettlauf auftreten, weil GetFragmentDataPtr den
 * Archetyp der ANDEREN Entitaet anfasst, waehrend ein anderer Thread sie verschieben kann.
 *
 * Mit dieser Tabelle liest die AvoidanceLoop nur noch (a) die Fragmente des eigenen Chunks und
 * (b) eine unveraenderliche Kopie. Das ist das Kriterium fuer ParallelForEachEntityChunk.
 *
 * Das Gitter allein reichte dafuer nicht: es liefert nur, WELCHE Entitaet in der Naehe ist, nicht
 * ihre Lage, Geschwindigkeit oder Ausdehnung.
 */
struct FUnitObstacleAgentSnapshot
{
	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Velocity = FVector::ZeroVector;

	float AgentRadius = 0.f;
	float ColliderRadius = 0.f;
	float PillHalfLength = 0.f;

	/** Gueltigkeitsmarke: ersetzt EntityManager.IsEntityValid, ohne den EntityManager anzufassen. */
	int32 SerialNumber = 0;

	/** 0 = Kreis, 1 = Pille. Als uint8 statt EMassColliderType, damit der Header leicht bleibt. */
	uint8 ColliderType = 0;

	bool bOccupied = false;
	bool bHasCollider = false;
	bool bCanAvoid = false;
	bool bIsMoving = true;
};

UCLASS()
class RTSUNITTEMPLATE_API UUnitObstacleSnapshotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Copy the grid's mutable state. Game thread only. */
	void Rebuild(const FNavigationObstacleHashGrid2D& Grid);

	/** Tabelle der Nachbardaten leeren. Spielthread. */
	void ResetAgents();

	/** Einen Nachbarn eintragen. Spielthread, waehrend des Schnappschusses. */
	void AddAgent(const FMassEntityHandle Entity, const FUnitObstacleAgentSnapshot& Data);

	/**
	 * Nachbardaten nachschlagen. Von JEDEM Thread sicher, sobald der Schnappschuss steht.
	 * Liefert nullptr, wenn die Entitaet beim Schnappschuss nicht vorhanden war - der Aufrufer
	 * behandelt das wie das frueher vorangestellte IsEntityValid == false.
	 */
	FORCEINLINE const FUnitObstacleAgentSnapshot* FindAgent(const FMassEntityHandle Entity) const
	{
		if (!Agents.IsValidIndex(Entity.Index))
		{
			return nullptr;
		}
		const FUnitObstacleAgentSnapshot& Entry = Agents[Entity.Index];
		// Der Seriennummernvergleich faengt den Fall ab, dass der Index seit dem Schnappschuss an
		// eine ANDERE Entitaet vergeben wurde. Ohne ihn wuerde ein Nachbar mit den Daten eines
		// laengst zerstoerten Vorgaengers ausweichen.
		return (Entry.bOccupied && Entry.SerialNumber == Entity.SerialNumber) ? &Entry : nullptr;
	}

	/** Falsch, solange die Tabelle noch nie gefuellt wurde. */
	FORCEINLINE bool HasAgentTable() const { return bAgentsBuilt; }

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

	/**
	 * Indiziert ueber FMassEntityHandle::Index statt als TMap: ein Feldzugriff statt eines
	 * Hashlaufs. Die Schleife schlaegt bis zu MaxObstacleResults Nachbarn je Einheit und Bild
	 * nach - bei 510 Einheiten sind das Zehntausende Zugriffe, da zaehlt der Unterschied.
	 */
	TArray<FUnitObstacleAgentSnapshot> Agents;
	bool bAgentsBuilt = false;
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
