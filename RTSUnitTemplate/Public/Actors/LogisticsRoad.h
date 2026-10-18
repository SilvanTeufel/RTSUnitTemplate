// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogisticsRoad.generated.h"

class ABuildingBase;
class USplineMeshComponent;
class UDecalComponent;
class UStaticMesh;
class UMaterialInterface;
class URuntimeVirtualTexture;
class UInstancedStaticMeshComponent;

/**
 * Per-instance custom data of the RVT writer quads (ALogisticsRoad::AddRVTPolyline). The writer material
 * reads them with PerInstanceCustomData nodes - keep both in sync. All distances in uu, world space XY.
 */
namespace RVTCustomData
{
	enum : int32
	{
		StartX,         // centre line point where this quad's own stretch of road begins
		StartY,
		DirX,           // unit direction of the road along this quad
		DirY,
		Length,         // length of the quad's own stretch (without the miter overhang)
		VStart,         // distance along the road at Start
		StartMiterTan,  // tan(half turn) at the start corner, + = turning towards +Y; 0 = square cut
		EndMiterTan,    // tan(half turn) at the end corner
		HalfWidth,      // half the road width
		TotalLength,    // length of the whole road (V at its end)
		FadeInLength,   // the road fades in over this length at its start (0 = hard start)
		FadeOutLength,  // ... and out over this length at its end
		VisibleFrom,    // growth: only V in VisibleFrom .. VisibleTo is drawn, with soft edges RevealSoftness long
		VisibleTo,      //   (whole road: -1e6 .. 1e7)
		RevealSoftness,
		HalfWidthStart, // merged roads: half width at the quad's start and end (the road widens smoothly);
		HalfWidthEnd,   //   HalfWidth stays the plain road's (texture tiling)
		Num
	};
}

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** A supply road: from a collecting base (Source) to a Converter (Sink). Drawn with the Supply* materials. */
	UPROPERTY(ReplicatedUsing = OnRep_RoadGeometry, VisibleInstanceOnly, BlueprintReadOnly, Category = Logistics)
	bool bIsSupplyRoad = false;

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

	/** Draw the ground lane as one continuous decal strip from building edge to building edge
	 *  (instead of the mesh pieces above). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal")
	bool bGroundLaneAsDecal = false;

	/** Deferred decal material. The texture repeats once per GroundDecalPieceLength along the road. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal", meta = (EditCondition = "bGroundLaneAsDecal"))
	TObjectPtr<UMaterialInterface> GroundDecalMaterial;

	/** Decal material of a supply road (to a Converter). Empty = GroundDecalMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal", meta = (EditCondition = "bGroundLaneAsDecal"))
	TObjectPtr<UMaterialInterface> SupplyGroundDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal", meta = (ClampMin = "10", EditCondition = "bGroundLaneAsDecal"))
	float GroundDecalWidth = 240.f;

	/** Length of one decal piece; the road is cut into whole pieces of about this length. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal", meta = (ClampMin = "10", EditCondition = "bGroundLaneAsDecal"))
	float GroundDecalPieceLength = 480.f;

	/** Projection depth (up and down from the ground). Keep it small: everything inside it that receives
	 *  decals gets the road painted on, units walking on it included. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal", meta = (ClampMin = "1", EditCondition = "bGroundLaneAsDecal"))
	float GroundDecalDepth = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|Decal", meta = (EditCondition = "bGroundLaneAsDecal"))
	int32 GroundDecalSortOrder = 0;

	/** Paint the ground lane into the terrain through a Runtime Virtual Texture (like the Xeno creep) instead of
	 *  decals: one seamless strip that follows the ground, no projection depth, no seams between pieces.
	 *  Needs an RVT volume in the level whose texture is TargetVirtualTexture, and a landscape that samples it.
	 *  Takes precedence over bGroundLaneAsDecal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT")
	bool bGroundLaneAsRVT = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (EditCondition = "bGroundLaneAsRVT"))
	TObjectPtr<URuntimeVirtualTexture> TargetVirtualTexture;

	/** Surface material with a Runtime Virtual Texture Output node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (EditCondition = "bGroundLaneAsRVT"))
	TObjectPtr<UMaterialInterface> RVTRoadMaterial;

	/** RVT material of a supply road (to a Converter). Empty = RVTRoadMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (EditCondition = "bGroundLaneAsRVT"))
	TObjectPtr<UMaterialInterface> SupplyRVTRoadMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (ClampMin = "10", EditCondition = "bGroundLaneAsRVT"))
	float RVTRoadWidth = 192.f;

	/**
	 * Maximum length of one writer quad. Quads always end at the corners of the (smoothed) polyline and
	 * are mitred there, so straight stretches need no splitting - the writer material works in road space.
	 * Only very long segments are split, to keep the quads' bounds (and the RVT pages they invalidate) small.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (ClampMin = "10", EditCondition = "bGroundLaneAsRVT"))
	float RVTPieceLength = 2000.f;

	/** Flat quad written into the RVT (+Z up, 100 x 100 uu). Default: engine plane. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (EditCondition = "bGroundLaneAsRVT"))
	TObjectPtr<UStaticMesh> RVTPlaneMesh;

	/**
	 * The road runs on under the building at both ends, up to its centre - under its WorkArea / BuildArea /
	 * ResourceArea too - instead of stopping at the building's edge.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (EditCondition = "bGroundLaneAsRVT"))
	bool bRVTRunUnderBuildings = true;

	/**
	 * At a building the road fades out softly over its last RVTEndFadeLength uu. Never longer than the stretch
	 * under the building, so the fade stays hidden under it. Needs a translucent writer material.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (ClampMin = "0", EditCondition = "bGroundLaneAsRVT"))
	float RVTEndFadeLength = 400.f;

	/**
	 * Draw order in the RVT against other writers (area decals: 0). Below 0 = the road lies under them,
	 * so areas stay on top where it runs under a building.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT", meta = (EditCondition = "bGroundLaneAsRVT"))
	int32 RVTSortPriority = -10;

	// --- Growth (RVT road): a new road is laid from the source to the sink, a retired one is taken up again ---

	/** Speed (uu/s) a new road is laid from the source towards the sink (0 = it appears at once). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Growth", meta = (ClampMin = "0", EditCondition = "bGroundLaneAsRVT"))
	float RoadGrowSpeed = 0.f;

	/**
	 * Speed (uu/s) a retired road is taken up again: back to the source, or into the sink when the source is
	 * gone (0 = it goes at once).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Growth", meta = (ClampMin = "0", EditCondition = "bGroundLaneAsRVT"))
	float RoadRetractSpeed = 0.f;

	/** Length of the soft edge at the growing or retreating end (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Growth", meta = (ClampMin = "1", EditCondition = "bGroundLaneAsRVT"))
	float RoadRevealSoftness = 150.f;

	/**
	 * With growth the writer is cut into pieces of about this length (uu). A step of the growth redraws only
	 * the piece at the moving end in the virtual texture: shorter = cheaper steps, but more components.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Growth", meta = (ClampMin = "200", EditCondition = "bGroundLaneAsRVT"))
	float RoadRevealPieceLength = 1000.f;

	/** Steps per second while the road grows or retreats. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Growth", meta = (ClampMin = "1", ClampMax = "60", EditCondition = "bGroundLaneAsRVT"))
	float RoadRevealRate = 20.f;

	/** Server world time the road began to grow (< 0 = laid at once). Set once, when the road is set up. */
	UPROPERTY(ReplicatedUsing = OnRep_RoadReveal, VisibleInstanceOnly, Category = "Logistics|Visual|RVT|Growth")
	double RoadGrowStartTime = -1.0;

	/** Server world times of taking the road up (< 0 = not retiring). The server destroys it at RoadRetractEndTime. */
	UPROPERTY(ReplicatedUsing = OnRep_RoadReveal, VisibleInstanceOnly, Category = "Logistics|Visual|RVT|Growth")
	double RoadRetractStartTime = -1.0;

	UPROPERTY(ReplicatedUsing = OnRep_RoadReveal, VisibleInstanceOnly, Category = "Logistics|Visual|RVT|Growth")
	double RoadRetractEndTime = -1.0;

	/** Taken up into the sink (the source is gone) instead of back to the source. */
	UPROPERTY(ReplicatedUsing = OnRep_RoadReveal, VisibleInstanceOnly, Category = "Logistics|Visual|RVT|Growth")
	bool bRoadRetractToSink = false;

	// --- Merging (RVT road): roads of a team that run on top of each other become one wider road ---

	/**
	 * Where this road runs on top of an older road of its team (same material) for at least MergeMinLength,
	 * it is not drawn - it ends in a clean junction - and the older road gets wider there instead. Only the
	 * look: the units still drive their own lanes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (EditCondition = "bGroundLaneAsRVT"))
	bool bMergeOverlappingRoads = false;

	/** Two roads overlap where their centre lines are closer than this share of RVTRoadWidth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (ClampMin = "0.1", ClampMax = "2", EditCondition = "bMergeOverlappingRoads"))
	float MergeOverlapDistance = 0.75f;

	/** Shorter overlaps (a crossing) stay drawn (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (ClampMin = "0", EditCondition = "bMergeOverlappingRoads"))
	float MergeMinLength = 400.f;

	/** Extra width per road merged into a stretch, relative to RVTRoadWidth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (ClampMin = "0", EditCondition = "bMergeOverlappingRoads"))
	float MergeWidthPerRoad = 0.35f;

	/** Widest a merged stretch gets, relative to RVTRoadWidth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (ClampMin = "1", EditCondition = "bMergeOverlappingRoads"))
	float MergeMaxWidthScale = 2.f;

	/**
	 * Where a road merges into an older one, it bends onto the older road's centre line over this length
	 * (uu) - a clean junction curve, after which it lies hidden under the older road (only the look).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (ClampMin = "0", EditCondition = "bMergeOverlappingRoads"))
	float MergeJoinLength = 400.f;

	/** The road widens and narrows over this length (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual|RVT|Merge", meta = (ClampMin = "0", EditCondition = "bMergeOverlappingRoads"))
	float MergeWidthBlendLength = 300.f;

	/** Server world time the road was first set up - the older road is kept where two merge. */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Logistics|Visual|RVT|Merge")
	double RoadOrderTime = -1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	bool bShowAirLane = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UStaticMesh> AirLaneMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UMaterialInterface> AirLaneMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	FVector2D AirLaneScale = FVector2D(0.06f, 0.06f);

	/** Ground lane material of a supply road (to a Converter). Empty = GroundLaneMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UMaterialInterface> SupplyGroundLaneMaterial;

	/** Air lane material of a supply road. Empty = AirLaneMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Visual")
	TObjectPtr<UMaterialInterface> SupplyAirLaneMaterial;

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

	/**
	 * true: logistics units drive this road. false: the road moves the resources itself (ALogisticsPipeline) -
	 * the dispatcher never sends a unit on it.
	 */
	virtual bool UsesCarriers() const { return true; }

	/**
	 * Server: the subsystem no longer needs this road. A road goes at once (with RoadRetractSpeed it is taken
	 * up first); a pipeline pulls back first.
	 */
	virtual void Retire();

	/** Called after the visuals were rebuilt (server and clients). Hook for extra decoration. */
	UFUNCTION(BlueprintImplementableEvent, Category = Logistics)
	void OnRoadVisualsRebuilt();

protected:
	UFUNCTION()
	void OnRep_RoadGeometry();

	UFUNCTION()
	void OnRep_RoadReveal();

	/** Clients: writes the visible stretch into the writer pieces; keeps stepping while the road grows or retreats. */
	void UpdateRoadReveal();

	/** The visible stretch of the road now (V from .. to); true while it still moves. */
	bool GetRoadRevealRange(float& OutFrom, float& OutTo) const;

	/** One writer piece: custom data and visibility for the visible stretch From .. To. */
	void ApplyRoadReveal(int32 Piece, float From, float To);

	double GetRoadServerTime() const;

	/** Server: destroys the road once it is taken up. */
	void OnRoadRetractFinished();

	FTimerHandle RoadRevealTimer;
	FTimerHandle RoadRetractTimer;

	/** Server: InitializeRoad set the geometry; called before the visuals are built. */
	virtual void OnRoadInitialized() {}

	virtual void RebuildVisuals();
	virtual void ClearVisuals();
	void UpdateTeamVisibility();
	void AddSegment(const FVector& Start, const FVector& End, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector2D& Scale);

	/** Lays dashes (or one piece per segment when Gap <= 0) along a polyline, carrying the pattern across corners. */
	void AddDashedPolyline(const TArray<FVector>& Points, float Dash, float Gap, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector2D& Scale);

	/** Lays decal pieces of equal length along a ground-hugging polyline. */
	void AddDecalPolyline(const TArray<FVector>& Points, UMaterialInterface* Material);

	/**
	 * Smooths the polyline into RVTPath and draws it into the Runtime Virtual Texture (with merging the
	 * network solve draws it - see RebuildVisuals).
	 */
	void AddRVTPolyline(const TArray<FVector>& Points, UMaterialInterface* Material, float FadeInLength = 0.f, float FadeOutLength = 0.f);

	/** Lays short quads along RVTPath that write into the Runtime Virtual Texture (RVTVisible / RVTHalfWidths if set). */
	void DrawRVT();

	/** Destroys the RVT writer pieces. */
	void ClearRVTWriters();

	/**
	 * Clients: which stretches of the team's RVT roads are drawn and how wide - every road against the older
	 * ones. Redraws the roads whose result changed. Leaving: a road that is going away (left out).
	 */
	static void SolveRoadNetwork(UWorld* World, int32 InTeamId, const ALogisticsRoad* Leaving);

	/** The smoothed centre line of the RVT road (clients). */
	TArray<FVector> RVTPath;

	/** RVTPath bent onto the older roads where it merges into them (merging); empty = RVTPath. */
	TArray<FVector> RVTDrawPath;

	/** Per point of RVTPath: drawn (merging); empty = all. */
	TArray<uint8> RVTVisible;

	/** Per point of RVTPath: half width (merging); empty = RVTRoadWidth / 2. */
	TArray<float> RVTHalfWidths;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RVTPathMaterial;

	float RVTFadeInLength = 0.f;
	float RVTFadeOutLength = 0.f;

	/** DrawRVT ran since the path was set. */
	bool bRVTDrawn = false;

	/**
	 * Merging: age rank in the team (0 = oldest). The writer sorts RVTSortPriority - rank, so an older road
	 * is drawn over a younger one - at a crossing the younger runs under it, at a junction its end is hidden.
	 */
	int32 RVTRank = 0;

	/** The RVT writer: one piece, or with growth one per RoadRevealPieceLength. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> RVTComponents;

	/** V range (start, end) of every writer piece. */
	TArray<FVector2f> RVTPieceRanges;

	/** Last written VisibleFrom, VisibleTo, RevealSoftness per piece - unchanged pieces are not redrawn. */
	TArray<FVector3f> RVTPieceReveal;

	/** V at the road's end. */
	float RVTTotalLength = 0.f;

	/** Seen by the local player's team (bOnlyVisibleToOwnTeam). */
	bool bTeamVisible = true;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> SegmentComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> DecalComponents;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
};
