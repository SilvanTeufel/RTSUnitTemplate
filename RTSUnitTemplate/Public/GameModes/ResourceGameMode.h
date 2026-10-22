// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSGameModeBase.h"
#include "Core/WorkerData.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "ResourceGameMode.generated.h"

/** Per-team logistics: lets one faction use a different road class, e.g. ALogisticsPipeline instead of carriers. */
USTRUCT(BlueprintType)
struct FTeamLogisticsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Logistics)
	int32 TeamId = 0;

	/** Road actor of this team's links. Empty = LogisticsRoadClass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Logistics)
	TSubclassOf<class ALogisticsRoad> RoadClass;
};

/**
 * Procedural resources: at game start every resource place of the map is removed and new ones are spawned
 * in clusters on the navmesh (server only; the spawned areas replicate).
 */
USTRUCT(BlueprintType)
struct FProceduralSettings
{
	GENERATED_BODY()

	/** On: remove the map's resource places at game start and spawn new ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural)
	bool Procedural = false;

	/**
	 * Most resource places per cluster. A cluster gets MinClusterSize..ClusterSize places at random; start
	 * clusters and those of AResourceClusterMarker always ClusterSize. How they lie: the ...ShapeWeight below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "1", ClampMax = "20", DisplayName = "Cluster Size (Max)"))
	int32 ClusterSize = 4;

	/** Fewest resource places per cluster (see ClusterSize). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "1", ClampMax = "20"))
	int32 MinClusterSize = 3;

	/**
	 * Cluster shape (relative weights; 0 = never): Ring - on a ring (3 = triangle, 4 = square, more than 6: one in
	 * the centre); Triangle - 3 corners, 4 = with one in the centre, 5-6 with places on the edges; Line - in a
	 * slightly bent row. Places are always ClusterSpacing apart. All 0: Ring. Start clusters: the same at every base.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float RingShapeWeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float TriangleShapeWeight = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float LineShapeWeight = 0.2f;

	/** Distance between two resource places of a cluster (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "50"))
	float ClusterSpacing = 400.f;

	/**
	 * Random offset of every resource place from its spot in the shape, as a share of ClusterSpacing (0.2 = up
	 * to 80 uu at 400) - looks less orderly. A place stays on the navmesh and at least ClusterSpacing x
	 * (1 - ClusterJitter) from every other place; if no offset fits, it keeps its spot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0", ClampMax = "0.45"))
	float ClusterJitter = 0.2f;

	/** How often a cluster is of this type (relative weights; 0 = never) - only when all ...ClusterWeight below are 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float PrimaryFrequency = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float SecondaryFrequency = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float TertiaryFrequency = 0.2f;

	/**
	 * Cluster mix: how often a cluster holds these resources (relative weights; 0 = never). Every resource of
	 * the mix gets at least one place, the other places are shared out at random. A resource without a class
	 * drops out of the mix. Start clusters get the same mixes at every base, the first one only Primary
	 * (bFirstStartClusterPrimaryOnly), the others by these weights.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float PrimaryOnlyClusterWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float PrimarySecondaryClusterWeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float PrimaryTertiaryClusterWeight = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float SecondaryTertiaryClusterWeight = 0.1f;

	/** Resource density: clusters per 10000 x 10000 uu (100 m x 100 m) of the navmesh's bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float ClusterDensity = 4.f;

	/** Upper limit of clusters, whatever the density says. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	int32 MaxClusters = 60;

	/** Minimum distance between two cluster centres (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float MinClusterDistance = 1600.f;

	/** Minimum distance of a cluster centre to player starts and placed buildings (uu). Not for the start clusters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float MinDistanceToBases = 2000.f;

	/**
	 * Clusters at every main base (the CollectAndStore building next to a player start; without one the player
	 * start itself), the same for everyone: the first only Primary (bFirstStartClusterPrimaryOnly), the others a
	 * mix by the ...ClusterWeight (all 0: the next type with a class and a frequency above 0). Spawned before the
	 * others.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0", ClampMax = "6"))
	int32 StartClusters = 2;

	/** The first start cluster holds only Primary; off: it gets a mix by the ...ClusterWeight like the others. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural)
	bool bFirstStartClusterPrimaryOnly = true;

	/**
	 * Least distance of a start cluster's places to the main base (uu) - keep it above the main building's
	 * WorkArea ResourcePlacementDistance, or it cannot be placed at the start.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float StartClusterMinDistance = 1100.f;

	/** The start clusters lie up to this much farther out (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float StartClusterSpread = 400.f;

	/** Least distance of a start cluster's places to the other buildings and the player starts (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float StartClusterClearance = 600.f;

	/** A main base belongs to a player start up to this distance (uu); farther, the start clusters go round the player start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float MainBaseReach = 4000.f;

	/**
	 * Open-ground test around a cluster centre: the navmesh must reach this far in every direction without
	 * a wall in between - no clusters in passages or narrow spots (uu, added to the cluster's own size).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float OpenRadius = 550.f;

	/**
	 * Every resource place keeps this distance to the edge of the navmesh (cliffs, rocks, holes) in 8 directions
	 * (uu). The cluster is turned until all its places fit; if none does, the spot is not used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "0"))
	float MinEdgeDistance = 400.f;

	/** Most height difference of the navmesh within the open-ground test - no clusters on ramps or slopes (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural, meta = (ClampMin = "1"))
	float MaxHeightDifference = 80.f;

	/** Classes to spawn per type. Empty = the classes of the removed resource places of that type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural)
	TArray<TSubclassOf<AWorkArea>> PrimaryClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural)
	TArray<TSubclassOf<AWorkArea>> SecondaryClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural)
	TArray<TSubclassOf<AWorkArea>> TertiaryClasses;

	/** Random seed (0 = a new layout every game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Procedural)
	int32 Seed = 0;
};

/**
 * GameMode class that manages resources for teams in a more generic way.
 */

USTRUCT(BlueprintType)
struct FWorkAreaArrays
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> PrimaryAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> SecondaryAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> TertiaryAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> RareAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> EpicAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> LegendaryAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<ABuildingBase*> BaseAreas;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<AWorkArea*> BuildAreas;
};

UCLASS()
class RTSUNITTEMPLATE_API AResourceGameMode : public ARTSGameModeBase
{
	GENERATED_BODY()

public:
	// Constructor
	AResourceGameMode();

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveBaseFromGroup(ABuildingBase* BuildingBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddBaseToGroup(ABuildingBase* BuildingBase);
	
protected:
	virtual void BeginPlay() override; // Override BeginPlay

	/** Before any actor's BeginPlay: the procedural resources replace the map's ones before workers pick them. */
	virtual void StartPlay() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Work)
	int MaxResourceAreasToSet = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Work)
	int MaxBuildAreasToSet = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Work)
	float HighestMaxResource = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Work)
	TMap<EResourceType, bool> SupplyLikeResources;
	
	// Helper function to initialize resource arrays
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitializeResources(int32 NumberOfTeams);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GatherWorkAreas();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GatherBases();

	/** Procedural resources (see FProceduralSettings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Procedural")
	FProceduralSettings ProceduralSettings;

	/**
	 * Server, game start: removes the map's resource places and spawns the clusters (those of the
	 * AResourceClusterMarker first). bMarkersOnly: only the markers' clusters, the map's places stay.
	 */
	void SpawnProceduralResources(bool bMarkersOnly = false);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AssignWorkAreasToWorkers();

public:
	// --- Logistics (roads between CollectOnly bases and the bases that store) -------------------
	// Read by ULogisticsSubsystem on the server. Without any CollectOnly base or logistics unit in
	// the level none of this does anything.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	bool bEnableLogistics = true;

	/** Road actor spawned per CollectOnly base. Subclass ALogisticsRoad in Blueprint for meshes / materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	TSubclassOf<class ALogisticsRoad> LogisticsRoadClass;

	/** Overrides LogisticsRoadClass per team, e.g. organic pipelines for the Xeno. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	TArray<FTeamLogisticsSettings> TeamLogisticsSettings;

	/** The road class of TeamId's links: its TeamLogisticsSettings entry, else LogisticsRoadClass. Never null. */
	UClass* GetLogisticsRoadClass(int32 TeamId) const;

	/** How often the dispatcher looks at the logistics units (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0.05"))
	float LogisticsTickInterval = 0.25f;

	/** How often the road network is re-checked even without a base appearing or dying (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0.5"))
	float LogisticsRoadRefreshInterval = 2.f;

	/** A logistics unit only sets off when at least this much (or its full capacity, if smaller) is waiting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	float LogisticsMinPickupAmount = 10.f;

	/**
	 * A Converter is supplied over the shortest route by a supplier that has the input (a Collector next door
	 * before the far main base). It keeps its supplier (and the road / pipeline to it) as long as that one has
	 * some of the input, and this long (seconds) after it ran dry - stops the road from being torn down and built
	 * again between two suppliers that take turns.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	float LogisticsSupplierSwitchDelay = 20.f;

	/** A well stocked supplier takes over from the current one when its route is this much shorter (0.15 = 15 %). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0", ClampMax = "0.95"))
	float LogisticsSupplierSwitchGain = 0.15f;

	/** Straight-line limit for a road. 0 = any distance. A CollectOnly base without a sink in range gets no road. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	float LogisticsMaxRoadLength = 0.f;

	/** Height of the air lane above the higher of its two ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	float LogisticsAirLaneHeight = 600.f;

	/** Distance from a base's edge at which roads end and units load / unload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	float LogisticsEndMargin = 250.f;

	/** Gap between a logistics unit and the base's edge while it loads / unloads (0 = touching; negative pulls it in
	 *  when the base's footprint is drawn larger than it looks). Units drive past the road end up to this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "-500"))
	float LogisticsDockGap = 0.f;

	/**
	 * Weight of a Converter supply run against an ordinary haul when an idle logistics unit picks its
	 * next job (both are scored by amount and distance). 1 = equal, higher = Converters are supplied first.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	float LogisticsConverterSupplyPriority = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Work)
	float ResourceDistanceMultiplier = 2.0f;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	ABuildingBase* GetClosestBaseFromArray(AWorkingUnitBase* Worker, const TArray<ABuildingBase*>& Bases);

	/** GetClosestBaseFromArray, measured from Location instead of the worker's resource place. */
	ABuildingBase* GetClosestBaseToLocation(AWorkingUnitBase* Worker, const TArray<ABuildingBase*>& Bases, const FVector& Location);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWorkArea* GetClosestWorkArea(AWorkingUnitBase* Worker, const TArray<AWorkArea*>& WorkAreas);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Work)
	FWorkAreaArrays WorkAreaGroups; // Storage for work areas grouped by type
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = Work)
	int32 NumberOfTeams = 10;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<AWorkArea*> GetFiveClosestResourcePlaces(AWorkingUnitBase* Worker);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<AWorkArea*> GetClosestBuildPlaces(AWorkingUnitBase* Worker);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWorkArea* GetRandomClosestWorkArea(const TArray<AWorkArea*>& WorkAreas);
	// Function to modify a resource for a specific team
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ModifyResource(EResourceType ResourceType, int32 TeamId, float Amount);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ModifyMaxResource(EResourceType ResourceType, int32 TeamId, float Amount);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void IncreaseMaxResources(const FBuildingCost& CapacityIncrease, int32 TeamId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DecreaseMaxResources(const FBuildingCost& CapacityDecrease, int32 TeamId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float GetMaxResource(EResourceType ResourceType, int TeamId);

	/** Which resource types behave like supply (a cap you occupy) rather than a stock you spend. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsSupplyLikeResource(EResourceType ResourceType) const;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool ModifyResourceCCost(const FBuildingCost& ConstructionCost, int32 TeamId);
	
	virtual float GetResource(int32 TeamId, EResourceType ResourceType) const override;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = Work)
	TArray<FResourceArray> TeamResources;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CanAffordConstruction(const FBuildingCost& ConstructionCost, int32 TeamId) const;

	/** Checks if a specific resource type can be afforded */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CanAffordResource(EResourceType ResourceType, float Amount, int32 TeamId) const;

	/** Similar to CanAffordConstruction, but returns an array of resources that the team is missing or cannot afford */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CanAffordConstructionExtended(const FBuildingCost& ConstructionCost, int32 TeamId, TArray<EResourceType>& OutMissingResources) const;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AssignWorkAreasToWorker(AWorkingUnitBase* Worker);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWorkArea* GetSuitableWorkAreaToWorker(int TeamId, const TArray<AWorkArea*>& WorkAreas);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<AWorkArea*> GetClosestResourcePlaces(AWorkingUnitBase* Worker);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<AWorkArea*> GetAllResourcePlaces(AWorkingUnitBase* Worker);

	// --- Per-worker / per-base resource permissions ----------------------------------------
	// The resource type the worker should currently be routed by: what it is carrying if it
	// carries anything, otherwise the type of its assigned ResourcePlace, otherwise MAX
	// ("unknown" - callers then apply no base filtering).
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	EResourceType GetWorkerRoutingResourceType(AWorkingUnitBase* Worker) const;

	// True if ANY living base of TeamId accepts ResourceType. Used so a worker never starts
	// gathering something it could not hand in anywhere. Returns true when the team has no base
	// at all, so a base-less team keeps behaving exactly as before.
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	bool CanTeamDeliverResourceType(int32 TeamId, EResourceType ResourceType) const;

	// Drops every area the worker may not mine (AWorkingUnitBase::CanMineWorkArea) and every
	// area whose resource no base of the worker's team accepts. Single chokepoint applied inside
	// GetAllResourcePlaces and GetFiveClosestResourcePlaces, which together feed every automatic
	// ResourcePlace assignment.
	void FilterResourcePlacesForWorker(AWorkingUnitBase* Worker, TArray<AWorkArea*>& InOutAreas) const;

	// Nearest (to the worker) non-depleted resource of the given WorkArea type that still has a free
	// worker slot and is within Radius, excluding the worker's current ResourcePlace. Returns null if
	// none qualify -> the caller sends the worker Idle. Used for the "resource full -> switch / idle" rule.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWorkArea* GetNearestAvailableResourceOfTypeWithin(AWorkingUnitBase* Worker, TEnumAsByte<WorkAreaData::WorkAreaType> Type, float Radius = 3000.f);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType, float Amount);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 GetCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType) const;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddMaxWorkersForResourceType(int TeamId, EResourceType ResourceType, float Amount);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 GetMaxWorkersForResourceType(int TeamId, EResourceType ResourceType) const;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsWorkerDistributionSet(int TeamId) const;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType, float Amount);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAllCurrentWorkers(int TeamId);

	virtual void CheckWinLoseCondition(AUnitBase* DestroyedUnit = nullptr) override;

private:
	void CheckWinLoseConditionTimer();
};