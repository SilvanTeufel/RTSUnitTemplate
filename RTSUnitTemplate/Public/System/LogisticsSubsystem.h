// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/LogisticsData.h"
#include "Core/WorkerData.h"
#include "LogisticsSubsystem.generated.h"

class AAbilityUnit;
class AUnitBase;
class ABuildingBase;
class ALogisticsRoad;
class AResourceGameMode;
class ACustomControllerBase;

/** One cached ground route between two bases. */
struct FLogisticsRouteCache
{
	TArray<FVector> Points;
	bool bHasGroundLane = false;
	float Length = 0.f;
	FVector SourceLocation = FVector::ZeroVector;
	FVector SinkLocation = FVector::ZeroVector;
	double ComputedAt = 0.0;
};

/** What a logistics unit is carrying out. */
enum class ELogisticsJobKind : uint8
{
	/** Stock of a CollectOnly base (or the output of a Converter) to a storing base. */
	Haul,
	/** The input type of a Converter, from a collecting base to the Converter. */
	Supply,
	/** Supply the Converter no longer wants, back to the base it was loaded at. */
	Return,
};

/** Server-side bookkeeping for one logistics unit. */
struct FLogisticsJob
{
	TWeakObjectPtr<AUnitBase> Unit;
	ELogisticsJobState State = ELogisticsJobState::Idle;
	TWeakObjectPtr<ALogisticsRoad> Road;
	/** The base the unit is driving to (the road's source while picking up, a storing base while delivering). */
	TWeakObjectPtr<ABuildingBase> TargetBase;

	FVector IssuedTarget = FVector::ZeroVector;
	bool bHasIssuedTarget = false;
	double LastIssueTime = -1000.0;

	/** What this unit has claimed at its pick-up base, so two shuttles do not drive for the same load. */
	float ReservedAmount = 0.f;

	ELogisticsJobKind Kind = ELogisticsJobKind::Haul;

	/** Supply / Return: the one type this unit loads and carries. */
	EResourceType CargoType = EResourceType::MAX;

	/** Supply / Return: the base the cargo was (or will be) loaded at - where a return goes. */
	TWeakObjectPtr<ABuildingBase> PickupBase;

	/** Supply: the Converter the cargo is for. */
	TWeakObjectPtr<ABuildingBase> Converter;

	double StateStartTime = 0.0;
	double IdleSince = -1.0;
	double NextAssignTime = 0.0;
	double LastProgressTime = 0.0;
	float BestDistance = TNumericLimits<float>::Max();
	int32 Retries = 0;

	/** The player sent it back to work: assign a route even though it is still moving. */
	bool bResumeRequested = false;

	/** A flying unit - remembered, because IsFlying is switched off while it is landed. */
	bool bIsFlyer = false;

	/** Landed for loading / unloading; takes off again when it leaves that state. */
	bool bLanded = false;

	/** Landed on the roof (LogisticsBehavior = LandOnRoof): still flying, only FlyHeight was lowered. */
	bool bOnRoof = false;

	/** FlyHeight before the roof landing, restored on take-off. */
	float SavedFlyHeight = 0.f;
};

/**
 * Roads and logistics units.
 *
 * Roads: every CollectOnly base gets one road to the nearest base of its team that stores
 * (CollectAndStore / StoreOnly) - a star network, nothing to place by hand. Rebuilt when bases
 * appear, die or move (flying buildings), and checked every LogisticsRoadRefreshInterval.
 *
 * Converters: every Converter with a conversion gets a supply road from the nearest collecting base
 * (CollectAndStore, CollectOnly, CollectOnlyWithoutWorker) that can give its input type. Supply runs
 * compete with the ordinary hauls (weighted by LogisticsConverterSupplyPriority) until the Converter's
 * input is full. Switching the conversion sends supply that is on its way back where it came from.
 *
 * Dispatcher: idle logistics units take the road whose source holds the most unclaimed resources,
 * drive there, load, drive to the storing end, unload (credits the team), repeat. Moves go through
 * the same batch-move path the AI uses, so Mass, prediction and replication see an ordinary order.
 * If the player gives the unit an order of its own, the dispatcher steps back and resumes once the
 * unit has stood idle for LogisticsResumeDelay.
 *
 * Runs on the server only. Settings live on AResourceGameMode ("RTSUnitTemplate|Logistics").
 */
UCLASS()
class RTSUNITTEMPLATE_API ULogisticsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** A base appeared, died or changed type: rebuild the road network on the next tick. */
	void MarkRoadsDirty() { bRoadsDirty = true; }

	/**
	 * A building now blocks the navmesh at Bounds (AUnitBase::Multicast_RegisterBuildingAsObstacle).
	 * Roads and pipelines that run through it and do not end at it are routed around it - as soon as the
	 * navmesh has taken the obstacle in.
	 */
	void NotifyObstaclePlaced(const AActor* Building, const FBox& Bounds);

	/**
	 * A Converter switched its conversion. Supply runs for it that have not loaded yet are dropped,
	 * loaded ones turn back to the base they were loaded at. Rebuilds its supply road.
	 */
	void OnConversionChanged(ABuildingBase* Converter);

	/** Registers the unit if it is a logistics unit, otherwise forgets it. */
	void RefreshLogisticsUnit(AAbilityUnit* Unit);

	/**
	 * Sends a logistics unit back to work right away - the player right-clicked one of the team's
	 * bases with it selected. Ends a pause, continues an interrupted trip, delivers held cargo.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Logistics")
	void ResumeLogistics(AUnitBase* Unit);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Logistics")
	TArray<ALogisticsRoad*> GetRoadsOfTeam(int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Logistics")
	ELogisticsJobState GetJobState(const AUnitBase* Unit) const;

	/** Supply roads of a team (collecting base -> Converter). */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Logistics")
	TArray<ALogisticsRoad*> GetSupplyRoadsOfTeam(int32 TeamId) const;

	/** Footprint radius of a base (capsule or colliding bounds, whichever is larger). */
	static float GetBaseRadius(const ABuildingBase* Base);

private:
	void RefreshRoads(AResourceGameMode* GameMode);
	void RefreshSupplyRoads(AResourceGameMode* GameMode);

	/** Picks the best of up to three nearest candidates by route (ground lane first, then length). */
	ABuildingBase* PickBestRouteEnd(ABuildingBase* From, TArray<ABuildingBase*> Candidates, bool bFromIsSource,
		AResourceGameMode* GameMode, FLogisticsRouteCache& OutRoute);

	/** Spawns (or keeps) the road Source -> Sink in Map[Key]. Returns the road. */
	ALogisticsRoad* UpdateRoad(TMap<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ALogisticsRoad>>& Map, ABuildingBase* Key,
		ABuildingBase* Source, ABuildingBase* Sink, const FLogisticsRouteCache& Route, bool bSupplyRoad, AResourceGameMode* GameMode);
	void TickDispatcher(AResourceGameMode* GameMode);

	/** Links that move the resources themselves (ALogisticsPipeline): deliver what arrived, launch the next packet. */
	void TickPipelines(AResourceGameMode* GameMode);
	void DiscoverLogisticsUnits();
	void DrawDebug(AResourceGameMode* GameMode) const;
	void LogSummary() const;

	const FLogisticsRouteCache& GetOrComputeRoute(ABuildingBase* Source, ABuildingBase* Sink, AResourceGameMode* GameMode);

	/** Drops the cached routes that cross a newly placed building, once the navmesh is up to date. */
	void RerouteAroundObstacles(double Now);

	/** True when the polyline crosses Box (2D, grown by Margin). */
	static bool RouteCrossesBox(const TArray<FVector>& Points, const FBox& Box, float Margin);

	/** Pick-up / drop-off point on the edge of a base, facing Toward. */
	FVector GetBaseEdgePoint(const ABuildingBase* Base, const FVector& Toward, float Margin) const;

	/** Where Unit stands while it loads / unloads at Base: on the edge facing Toward, plus its own radius and LogisticsDockGap. */
	FVector GetDockPoint(const ABuildingBase* Base, const AUnitBase* Unit, const FVector& Toward) const;
	/** Distance from Base's centre at which Unit touches it (base radius + unit radius + LogisticsDockGap). */
	float GetDockDistance(const ABuildingBase* Base, const AUnitBase* Unit) const;

	/** A flyer with LogisticsBehavior = LandOnRoof at a base that allows it (bAllowRoofLanding): docks on the roof instead of at the edge. */
	static bool LandsOnRoof(const AUnitBase* Unit, const ABuildingBase* Base);
	/** Roof point a LandsOnRoof unit docks at: a fifth of the base radius from the centre, facing Toward. */
	FVector GetRoofDockPoint(const ABuildingBase* Base, const FVector& Toward) const;
	/** World Z of the roof of Base (RoofLandingHeight, else the top of its ISM / skeletal mesh, else its capsule). */
	float GetRoofZ(const ABuildingBase* Base, const FVector& Location) const;
	/** Lowers a LandsOnRoof unit onto the roof of Base (or lands it on the ground beside the base). */
	void LandUnit(FLogisticsJob& Job, AUnitBase* Unit);
	/** Undoes LandUnit. */
	static void TakeOffUnit(FLogisticsJob& Job, AUnitBase* Unit);
	static bool IsBaseAlive(const ABuildingBase* Base);
	/** Alive and CanMove == false: roads and pipelines only connect buildings that stay put. */
	static bool CanHaveRoad(const ABuildingBase* Base);
	static bool IsUnitAlive(const AUnitBase* Unit);

	ABuildingBase* FindNearestSink(int32 TeamId, const FVector& From) const;
	float GetReservedAt(const ABuildingBase* Source) const;

	/** What Source can still hand out of Type as Converter supply, after the claims of other units. */
	float GetSupplyAvailable(const ABuildingBase* Source, EResourceType Type, const AResourceGameMode* GameMode) const;

	/** Supply claimed for or carried to Converter by every unit except Exclude. */
	float GetInboundSupply(const ABuildingBase* Converter, const FLogisticsJob* Exclude = nullptr) const;

	/** The Converter of a supply job still wants its cargo type. */
	static bool IsSupplyStillWanted(const FLogisticsJob& Job);

	/** A base a returning unit may unload its cargo at. */
	static bool CanTakeReturn(const ABuildingBase* Base);

	ABuildingBase* FindNearestReturnBase(int32 TeamId, const FVector& From) const;

	void SetJobState(FLogisticsJob& Job, ELogisticsJobState NewState, double Now);
	void ReleaseReservation(FLogisticsJob& Job);
	bool IssueMove(FLogisticsJob& Job, const FVector& Target);
	bool HasArrived(const FLogisticsJob& Job, const FVector& UnitLocation) const;
	bool WasOverridden(const FLogisticsJob& Job) const;
	ACustomControllerBase* GetCommandController(int32 TeamId);

	void DoLoad(FLogisticsJob& Job, double Now);
	void DoUnload(FLogisticsJob& Job, double Now);
	bool StartDelivery(FLogisticsJob& Job, double Now, float Margin);

	/** Sends a supply unit with cargo back to where it loaded (or the nearest collecting base). */
	bool StartReturn(FLogisticsJob& Job, double Now, float Margin);

	/** Supply unit with cargo: drive (on) to the Converter. */
	bool StartSupplyDelivery(FLogisticsJob& Job, double Now, float Margin);

	void DoLoadSupply(FLogisticsJob& Job, double Now);
	void DoUnloadSupply(FLogisticsJob& Job, double Now);
	void DoUnloadReturn(FLogisticsJob& Job, double Now);

	/** Back to an ordinary hauler with nothing special to remember. */
	static void ResetJobKind(FLogisticsJob& Job);

	TArray<FLogisticsJob> Jobs;

	/** Road per CollectOnly base. */
	TMap<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ALogisticsRoad>> RoadsBySource;

	/** Supply road per Converter (its source is the collecting base that supplies it). */
	TMap<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ALogisticsRoad>> SupplyRoadsByConverter;

	/** Per Converter: since when (world seconds) its current supplier has had none of the input. */
	TMap<TWeakObjectPtr<ABuildingBase>, double> SupplierEmptySince;

	/** Ground routes per (source, sink), so a periodic refresh does not path-find again. */
	TMap<TPair<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ABuildingBase>>, FLogisticsRouteCache> RouteCache;

	TWeakObjectPtr<ACustomControllerBase> CachedController;

	/** Buildings placed since the last reroute, waiting for the navmesh (NotifyObstaclePlaced). */
	struct FPendingObstacle
	{
		TWeakObjectPtr<const AActor> Building;
		FBox Bounds;
		double Time = 0.0;
	};
	TArray<FPendingObstacle> PendingObstacles;

	bool bRoadsDirty = true;
	double LastRoadRefresh = -1000.0;
	double LastDispatch = -1000.0;
	double LastDiscovery = -1000.0;
	double LastSummary = -1000.0;
};
