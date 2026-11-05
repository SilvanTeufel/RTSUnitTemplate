// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FullProceduralMapComponent.generated.h"

class ABuildingBase;
class AUnitBase;
class UNavigationSystemV1;
class ANavigationData;

/** One actor the server moved for the full procedural map - replicated so clients move their copy too. */
USTRUCT(BlueprintType)
struct FRTSProceduralRelocation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Procedural")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Procedural")
	FVector Location = FVector::ZeroVector;
};

/** What clients need to rebuild the server's full procedural map (AResourceGameState::ProceduralMap). */
USTRUCT(BlueprintType)
struct FRTSProceduralMapState
{
	GENERATED_BODY()

	/** Seed of the terrain. 0 = no full procedural map. */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Procedural")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Procedural")
	TArray<FRTSProceduralRelocation> Relocations;
};

/**
 * Full procedural map, switched on in the game mode (bEnabled). At match start (StartPlay, server, before any
 * BeginPlay and before the procedural resources), in this order:
 *  1. a level actor with IRTSProceduralTerrain builds a new terrain (seed), the navmesh is built (blocking),
 *  2. every main base (the building next to a player start) moves to a random spot on a high plateau, on
 *     open, flat navmesh, the bases far apart and reachable from each other. Everything placed around a base
 *     (units, buildings, player starts, waypoints, markers ...) moves along; the player starts go over the base,
 *     so the camera starts there,
 *  3. the other placed actors of those kinds go to the nearest reachable navmesh,
 *  4. SpawnPointCount spawn points far from the human bases: the spawn tables keep their rows, only UnitOffset
 *     is replaced by one of these points (the same table position always gets the same point),
 *  5. the waypoints of the spawn tables follow the human main base,
 *  6. then the procedural resources (AResourceGameMode, only reachable spots) and the terrain's decoration.
 * Without a terrain actor in the level nothing changes.
 */
UCLASS(ClassGroup = (RTSUnitTemplate), meta = (BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UFullProceduralMapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFullProceduralMapComponent();

	/** On: build terrain, bases and spawn points at random at every match start (see the class comment). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural")
	bool bEnabled = false;

	/** Seed of the terrain and of the placement. 0 = a new map every match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural")
	int32 Seed = 0;

	/** A main base belongs to a player start up to this distance (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases", meta = (ClampMin = "0"))
	float MainBaseReach = 4000.f;

	/**
	 * Main bases only on the high ground: navmesh whose height lies in this top share of the map's height range
	 * (0.5 = upper half). If there is no fitting spot there, any height.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases", meta = (ClampMin = "0.05", ClampMax = "1"))
	float TopPlateauShare = 0.5f;

	/** The main bases take the highest level that has room and a way down (else the high ground at random). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases")
	bool bPreferHighestLevel = true;

	/** Least distance between two main bases (uu). Halved step by step if the map has no room for it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases", meta = (ClampMin = "0"))
	float MinBaseDistance = 12000.f;

	/** Open, flat navmesh in this radius around a main base's new spot (uu) - the base buildings stand on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases", meta = (ClampMin = "0"))
	float BaseOpenRadius = 1500.f;

	/** Most height difference of the navmesh within BaseOpenRadius (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases", meta = (ClampMin = "1"))
	float BaseMaxHeightDifference = 60.f;

	/** Placed actors up to this distance from a main base move along with it, keeping their offset (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases", meta = (ClampMin = "0"))
	float RelocateRadius = 6000.f;

	/**
	 * Besides units, buildings, player starts, waypoints, work areas, resource cluster markers and AI expansion
	 * sites, these classes are moved too (story triggers ...). Actors with the tag ProceduralRelocate as well.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Bases")
	TArray<TSubclassOf<AActor>> ExtraRelocateClasses;

	/** How many spawn points the spawn tables get. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Spawn", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SpawnPointCount = 3;

	/** Least distance of a spawn point to every human main base (uu). Lowered step by step if the map has no room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Spawn", meta = (ClampMin = "0"))
	float SpawnMinDistance = 10000.f;

	/** Least distance between two spawn points (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Spawn", meta = (ClampMin = "0"))
	float SpawnPointSpacing = 5000.f;

	/** Spawned units start this high above a spawn point (uu) - the spawn tables' UnitOffset.Z is replaced. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Spawn")
	float SpawnHeight = 100.f;

	/** The boss wave spawners go to the spawn points too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Spawn")
	bool bMoveBossSpawners = true;

	/** The waypoints the spawn tables send their units to (WaypointTag) follow the human main base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Spawn")
	bool bWaypointsFollowHumanBase = true;

	/** Grid of navmesh samples the spots are picked from (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Advanced", meta = (ClampMin = "200"))
	float SampleSpacing = 600.f;

	/** Clients: the terrain's decoration starts this long after the map came in (s), so resources and buildings are there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Full Procedural|Advanced", meta = (ClampMin = "0"))
	float ClientDecorationDelay = 3.f;

	static const FName RelocateTag;

	/** Server, StartPlay: terrain, navmesh, bases, spawn points (see the class comment). True when the map is procedural. */
	bool PrepareMap();

	/** Server, after the procedural resources: the terrain's decoration (IRTSProceduralTerrain::OnProceduralMapReady). */
	void FinishMap();

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Procedural")
	bool IsMapActive() const { return bActive; }

	/** True if a path leads from the first human main base to P (also true when the map is not procedural). */
	bool IsReachable(const FVector& P) const;

	/** The navmesh point nearest to Desired that is reachable (and open ground in OpenRadius, if > 0). */
	bool FindReachableNavPoint(const FVector& Desired, FVector& Out, float OpenRadius = 0.f) const;

	/** UnitOffset of a spawn table row -> one of the spawn points (the same offset always the same point). */
	FVector RemapSpawnOffset(const FVector& Offset) const;

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Procedural")
	const TArray<FVector>& GetSpawnPoints() const { return SpawnPoints; }

	/** Share of the navmesh a path leads to from the first human main base (0..1; 1 when the map is not procedural). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Procedural")
	float GetReachableShare() const { return ReachableShare; }

	/** Every actor implementing IRTSProceduralTerrain (also for clients). */
	static void FindTerrainProviders(UWorld* World, TArray<AActor*>& Out);

	/** Moves an actor at runtime (static roots become movable). */
	static void MoveActor(AActor* Actor, const FVector& Location);

private:
	bool IsOpenGround(const FVector& P, float Radius, float MaxHeightDifference) const;
	bool PathExists(const FVector& From, const FVector& To) const;
	bool IsRelocatable(const AActor* Actor) const;

	UPROPERTY(Transient)
	bool bActive = false;

	UPROPERTY(Transient)
	bool bTried = false;

	UPROPERTY(Transient)
	int32 UsedSeed = 0;

	UPROPERTY(Transient)
	TArray<FVector> SpawnPoints;

	UPROPERTY(Transient)
	float ReachableShare = 1.f;

	/** New positions of the human main bases; [0] is the anchor of IsReachable. */
	UPROPERTY(Transient)
	TArray<FVector> HumanBases;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Provider = nullptr;
};
