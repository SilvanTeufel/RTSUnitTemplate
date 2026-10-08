// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/LogisticsData.h"
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
};

/**
 * Roads and logistics units.
 *
 * Roads: every CollectOnly base gets one road to the nearest base of its team that stores
 * (CollectAndStore / StoreOnly) - a star network, nothing to place by hand. Rebuilt when bases
 * appear, die or move (flying buildings), and checked every LogisticsRoadRefreshInterval.
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

private:
	void RefreshRoads(AResourceGameMode* GameMode);
	void TickDispatcher(AResourceGameMode* GameMode);
	void DiscoverLogisticsUnits();
	void DrawDebug(AResourceGameMode* GameMode) const;
	void LogSummary() const;

	const FLogisticsRouteCache& GetOrComputeRoute(ABuildingBase* Source, ABuildingBase* Sink, AResourceGameMode* GameMode);

	/** Pick-up / drop-off point on the edge of a base, facing Toward. */
	FVector GetBaseEdgePoint(const ABuildingBase* Base, const FVector& Toward, float Margin) const;
	static float GetBaseRadius(const ABuildingBase* Base);
	static bool IsBaseAlive(const ABuildingBase* Base);
	static bool IsUnitAlive(const AUnitBase* Unit);

	ABuildingBase* FindNearestSink(int32 TeamId, const FVector& From) const;
	float GetReservedAt(const ABuildingBase* Source) const;

	void SetJobState(FLogisticsJob& Job, ELogisticsJobState NewState, double Now);
	void ReleaseReservation(FLogisticsJob& Job);
	bool IssueMove(FLogisticsJob& Job, const FVector& Target);
	bool HasArrived(const FLogisticsJob& Job, const FVector& UnitLocation, float Margin) const;
	bool WasOverridden(const FLogisticsJob& Job) const;
	ACustomControllerBase* GetCommandController(int32 TeamId);

	void DoLoad(FLogisticsJob& Job, double Now);
	void DoUnload(FLogisticsJob& Job, double Now);
	bool StartDelivery(FLogisticsJob& Job, double Now, float Margin);

	TArray<FLogisticsJob> Jobs;

	/** Road per CollectOnly base. */
	TMap<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ALogisticsRoad>> RoadsBySource;

	/** Ground routes per (source, sink), so a periodic refresh does not path-find again. */
	TMap<TPair<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ABuildingBase>>, FLogisticsRouteCache> RouteCache;

	TWeakObjectPtr<ACustomControllerBase> CachedController;

	bool bRoadsDirty = true;
	double LastRoadRefresh = -1000.0;
	double LastDispatch = -1000.0;
	double LastDiscovery = -1000.0;
	double LastSummary = -1000.0;
};
