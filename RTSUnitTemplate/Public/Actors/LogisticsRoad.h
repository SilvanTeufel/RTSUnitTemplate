// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogisticsRoad.generated.h"

class ABuildingBase;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * One road between a CollectOnly base (Source) and the base that stores its resources (Sink).
 *
 * Spawned, updated and removed by ULogisticsSubsystem on the server only - nobody places these by hand.
 * Every road has two lanes:
 *   - Ground: the navmesh path between the two bases. Missing when there is no path (island).
 *   - Air:    a straight line at AirHeight. Always present.
 * Ground logistics units only use roads that have a ground lane, flying ones use any road.
 *
 * Replicates the lane geometry; clients only build the visuals from it. Subclass in Blueprint to
 * pick meshes and materials (LogisticsRoadClass on the ResourceGameMode).
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ALogisticsRoad : public AActor
{
	GENERATED_BODY()

public:
	ALogisticsRoad();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Server: sets the whole road at once and rebuilds the visuals. */
	void InitializeRoad(ABuildingBase* InSource, ABuildingBase* InSink, int32 InTeamId,
		const TArray<FVector>& InGroundPoints, bool bInHasGroundLane, float InAirHeight);

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	TObjectPtr<ABuildingBase> SourceBase = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	TObjectPtr<ABuildingBase> SinkBase = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	int32 TeamId = 0;

	/** The ground lane, source end first. With no ground lane: just the two end points (for the air lane). */
	UPROPERTY(ReplicatedUsing = OnRep_RoadGeometry, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	TArray<FVector> GroundPoints;

	UPROPERTY(ReplicatedUsing = OnRep_RoadGeometry, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	bool bHasGroundLane = false;

	UPROPERTY(ReplicatedUsing = OnRep_RoadGeometry, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	float AirHeight = 600.f;

	/** Length of the ground lane (or the straight distance without one). Server only. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	float RoadLength = 0.f;

	// --- Look ---------------------------------------------------------------------------------

	/** Mesh stretched along each ground segment (+X forward). Default: engine cube, flattened by GroundLaneScale. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UStaticMesh> GroundLaneMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UMaterialInterface> GroundLaneMaterial;

	/** Y = width, Z = thickness, relative to the mesh (engine cube = 100 uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	FVector2D GroundLaneScale = FVector2D(0.18f, 0.03f);

	/** Draw the ground lane as a dashed line (like a flight route) instead of a solid strip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	bool bDashedGroundLane = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual", meta = (ClampMin = "10", EditCondition = "bDashedGroundLane"))
	float GroundDashLength = 140.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual", meta = (ClampMin = "0", EditCondition = "bDashedGroundLane"))
	float GroundGapLength = 90.f;

	/** Lifts the ground lane off the terrain so it does not z-fight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	float GroundLaneZOffset = 6.f;

	/** Ground segments are cut to this length and dropped onto the terrain, so the lane follows hills. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	float GroundSegmentLength = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	bool bShowAirLane = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UStaticMesh> AirLaneMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UMaterialInterface> AirLaneMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	FVector2D AirLaneScale = FVector2D(0.06f, 0.06f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual", meta = (ClampMin = "10"))
	float AirDashLength = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual", meta = (ClampMin = "0"))
	float AirGapLength = 150.f;

	/** Hide the road from players of other teams. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	bool bOnlyVisibleToOwnTeam = true;

	/** The pick-up point at the source base and the drop-off point at the sink, as used by the dispatcher. */
	UFUNCTION(BlueprintPure, Category = Logistics)
	FVector GetSourceEnd() const;

	UFUNCTION(BlueprintPure, Category = Logistics)
	FVector GetSinkEnd() const;

	/** Called after the visuals were rebuilt (server and clients). Hook for extra decoration. */
	UFUNCTION(BlueprintImplementableEvent, Category = Logistics)
	void OnRoadVisualsRebuilt();

protected:
	UFUNCTION()
	void OnRep_RoadGeometry();

	void RebuildVisuals();
	void ClearVisuals();
	void UpdateTeamVisibility();
	void AddSegment(const FVector& Start, const FVector& End, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector2D& Scale);

	/** Lays dashes (or one piece per segment when Gap <= 0) along a polyline, carrying the pattern across corners. */
	void AddDashedPolyline(const TArray<FVector>& Points, float Dash, float Gap, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector2D& Scale);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> SegmentComponents;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
};
