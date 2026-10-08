// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassEntityQuery.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "UnitFormationSubsystem.generated.h"

/**
 * Formation keeping while marching (AoE4 style), driven ONLY through speed.
 *
 * Idee: jede Einheit laeuft weiter ihren eigenen Pfad zu ihrem eigenen Formationsziel - das haelt
 * die Formation seitlich schon weitgehend zusammen. Auseinander zieht sich der Block nur in
 * Marschrichtung (unterschiedliche Tempi, Umwege um Hindernisse). Genau das regeln wir hier:
 *
 *   Soll-Platz   = Schwerpunkt der Gruppe + Formationsversatz (Ziel - Zielschwerpunkt)
 *   Laengsfehler = Abstand zum Soll-Platz entlang der Marschrichtung (positiv = zurueck)
 *   Tempo        = Gruppentempo (Langsamster) * clamp(1 + Fehler/Korrekturweg, MinSlow, MaxBoost)
 *
 * MoveTarget.Center wird NIE angefasst. Ein verschobenes Ziel loest im UnitMovementProcessor eine
 * neue Pfadsuche aus, und waehrend der steht die Einheit (DesiredVelocity = 0) - das war der
 * Grundfehler des Kundenansatzes in TemplateLab, der Slot-Ziele 1-2x je Sekunde neu verschickte.
 *
 * Ablauf je Bild:
 *   1. ExecuteBatchMove (Spielthread) legt Gruppen an -> Warteschlange (kritischer Abschnitt),
 *      weil der Befehl zeitgleich mit den Mass-Phasen laufen kann.
 *   2. UUnitFormationGatherProcessor (seriell, GameThread): Warteschlange einarbeiten,
 *      Schwerpunkt/Tempo je Gruppe aufsummieren, Seitenlenkung gegen das Navmesh pruefen.
 *   3. UUnitFormationSteerProcessor (ParallelForEachEntityChunk): liest eigene Fragmente plus die
 *      in Schritt 2 fertige Gruppentabelle und skaliert Steering.DesiredVelocity.
 * Keine Fremd-Entitaetszugriffe in der Parallelschleife -> kein CurrentArchetype-Wettlauf.
 */

/** Marks a unit that is currently marching as part of a formation group. */
USTRUCT()
struct RTSUNITTEMPLATE_API FUnitFormationMemberTag : public FMassTag
{
	GENERATED_BODY()
};

/** Per-entity membership, indexed by FMassEntityHandle::Index (same idea as the obstacle snapshot). */
struct FUnitFormationMember
{
	/** Formation offset relative to the group's target centroid (2D, world space). */
	FVector2f Offset = FVector2f::ZeroVector;

	/** Smoothed speed factor; written only by the steer pass for its own entity. */
	float SmoothedFactor = 1.f;

	int32 SerialNumber = 0;
	int32 GroupId = INDEX_NONE;

	/** Lateral steering toward the slot would leave the navmesh (navmesh raycast in the gather pass). */
	bool bLateralBlocked = false;
};

/** Per-group state. Accumulators are filled by the gather pass, the rest is read by the steer pass. */
struct FUnitFormationGroup
{
	/** Centroid of all member targets - the formation's "anchor" at the destination. */
	FVector2f TargetCentroid = FVector2f::ZeroVector;

	/** Largest |Offset| of the group; used to size the final approach phase. */
	float Radius = 0.f;

	// --- Result of the last gather pass (read-only for the steer pass) ---
	FVector2f Centroid = FVector2f::ZeroVector;
	FVector2f Heading = FVector2f(1.f, 0.f);
	float MinSpeed = 0.f;
	int32 ActiveCount = 0;
	bool bHasCentroid = false;
	/** True once the block is close to its destination - regulation then only boosts, never slows. */
	bool bFinalPhase = false;

	/**
	 * Mean (target factor - 1) over the members, subtracted again in the steer pass. The square-root
	 * response is not symmetric over a real distribution of errors; without this the whole block
	 * marched up to 15 % below its pace (measured: avg velocity 680 at group speed 800).
	 */
	float FactorBias = 0.f;

	/**
	 * Consecutive gather passes with fewer than two members. A new group is only removed after a
	 * grace period: the member tag is added deferred, so the first passes can see no member at all.
	 */
	int32 EmptyPasses = 0;

};

/** One queued membership change, produced on the game thread by a move command. */
struct FUnitFormationRequest
{
	FMassEntityHandle Entity;
	FVector2f Offset = FVector2f::ZeroVector;
	/** INDEX_NONE = remove the entity from any formation. */
	int32 GroupId = INDEX_NONE;
};

UCLASS()
class RTSUNITTEMPLATE_API UUnitFormationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Registers a new formation group for a move command. Game thread.
	 * @param Entities  units that received the move order
	 * @param Targets   their final (already nav-validated) destinations, same order
	 * @return the new group id, or INDEX_NONE if the group was too small / the feature is off
	 */
	int32 RegisterGroup(const TArray<FMassEntityHandle>& Entities, const TArray<FVector>& Targets);

	/** Removes the entities from any formation group. Game thread. */
	void UnregisterEntities(const TArray<FMassEntityHandle>& Entities);

	/** True when the feature is switched on (rts.formation.enable). */
	static bool IsFormationEnabled();

	/** Minimum number of units for a move order to form a group (rts.formation.mingroupsize). */
	static int32 GetMinGroupSize();

	// --- Used by the processors only ---

	/** Applies all queued requests. Called by the gather pass before it reads anything. */
	void ConsumePendingRequests();

	FORCEINLINE const FUnitFormationMember* FindMember(const FMassEntityHandle Entity) const
	{
		if (!Members.IsValidIndex(Entity.Index))
		{
			return nullptr;
		}
		const FUnitFormationMember& Entry = Members[Entity.Index];
		return (Entry.GroupId != INDEX_NONE && Entry.SerialNumber == Entity.SerialNumber) ? &Entry : nullptr;
	}

	/** Mutable variant for the steer pass. Each chunk only touches its own entities' entries. */
	FORCEINLINE FUnitFormationMember* FindMemberMutable(const FMassEntityHandle Entity)
	{
		return const_cast<FUnitFormationMember*>(FindMember(Entity));
	}

	void RemoveMember(const FMassEntityHandle Entity);

	FORCEINLINE const FUnitFormationGroup* FindGroup(const int32 GroupId) const { return Groups.Find(GroupId); }
	FORCEINLINE FUnitFormationGroup* FindGroupMutable(const int32 GroupId) { return Groups.Find(GroupId); }
	TMap<int32, FUnitFormationGroup>& GetGroupsMutable() { return Groups; }
	const TMap<int32, FUnitFormationGroup>& GetGroups() const { return Groups; }

private:
	/** Indexed by FMassEntityHandle::Index. Only resized inside ConsumePendingRequests. */
	TArray<FUnitFormationMember> Members;
	TMap<int32, FUnitFormationGroup> Groups;

	/** Group data waiting for the next gather pass (written on the game thread). */
	TMap<int32, FUnitFormationGroup> PendingGroups;
	TArray<FUnitFormationRequest> PendingRequests;
	FCriticalSection PendingLock;

	int32 NextGroupId = 0;
};

/**
 * Data access is synchronised by the processor order (gather -> steer) and the pending-request
 * lock, so Mass may run both processors off the game thread.
 */
template<>
struct TMassExternalSubsystemTraits<UUnitFormationSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};

/**
 * Pass 1: applies queued membership changes and builds the per-group centroid / heading / pace.
 * Serial, but not bound to the game thread - it reads only the entities' own fragments.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitFormationGatherProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitFormationGatherProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	/** Pass counter for spreading the navmesh checks over several frames. */
	uint32 NavCheckPass = 0;
};

/**
 * Pass 2: scales Steering.DesiredVelocity per unit so the block keeps its shape along the march
 * direction. Runs in parallel over chunks.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitFormationSteerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitFormationSteerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
