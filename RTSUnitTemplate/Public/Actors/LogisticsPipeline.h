// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Actors/LogisticsRoad.h"
#include "Core/WorkerData.h"
#include "LogisticsPipeline.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** One load of resources travelling through a pipeline. */
USTRUCT()
struct FPipelinePacket
{
	GENERATED_BODY()

	/** Server world time (AGameStateBase::GetServerWorldTimeSeconds) at which it left the source. */
	UPROPERTY()
	double SpawnTime = 0.0;

	UPROPERTY()
	EResourceType Type = EResourceType::Primary;

	UPROPERTY()
	float Amount = 0.f;

	/** On its way back to the source (the pipeline pulls back). SpawnTime is then mirrored: see ALogisticsPipeline::Retire. */
	UPROPERTY()
	bool bReturning = false;
};

/**
 * A link that moves the resources itself, without logistics units: an organic tube from building centre
 * to building centre (the Xeno). Pick it per team with AResourceGameMode::TeamLogisticsSettings.
 *
 * Server: ULogisticsSubsystem launches a packet every PacketInterval while the source has enough and
 * hands it to the sink after PacketTravelTime. Only the packet list replicates (it changes on launch and
 * arrival), never positions.
 *
 * Clients: the tube is one mesh built once from the replicated lane (one draw call, no tick). All motion -
 * the wobble and the bulge travelling with each packet - is world position offset in the material. It gets
 * the packets' start times and computes their positions from the material's Time itself.
 *
 * Material parameters (set on a dynamic instance): PipeLength, PacketSpeed, TubeRadius, Packet0..Packet7
 * (x = start time in the local world time, y = size 0..1.5, z = resource type, w = 1 while in flight),
 * GrowStart (local world time the tube began to grow), GrowSpeed (uu per second, 0 = fully grown, < 0 = pulling
 * back), GrowTail (1 = pulls back from the source end into the sink). Packet w = -1: flowing back to the source.
 * Mesh UVs (full precision):
 * - UV0: x = distance along the own tube in uu (roots continue from where they leave), y = around (0..1).
 * - UV1: x = distance along the main tube in uu (roots: where they leave it), y = 0 on the main tube,
 *        1 .. 2 along a root (2 = its tip in the ground).
 * - UV2: the main tube's sideways direction (world XY) - roots sway along with the tube where they leave it.
 * - UV3: x = radius of the tube or root at this ring (uu) - the growth pulls the not yet grown part onto the
 *        centre line.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ALogisticsPipeline : public ALogisticsRoad
{
	GENERATED_BODY()

public:
	ALogisticsPipeline();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool UsesCarriers() const override { return false; }

	/**
	 * Server: pulls the tube back instead of removing it at once, then destroys it. Into the source - the
	 * packets in it flow back there - or, when the source is gone, into the sink: the packets carry on and
	 * are delivered. Either way they are credited when the pipeline is gone (EndPlay).
	 */
	virtual void Retire() override;

	/** The number of packet slots of the material. */
	static constexpr int32 MaxPacketSlots = 8;

	// --- Transport (server) ---------------------------------------------------------------------

	/** Most a packet carries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline", meta = (ClampMin = "1"))
	float PacketAmount = 20.f;

	/** Seconds between two packets leaving the source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline", meta = (ClampMin = "0.1"))
	float PacketInterval = 2.f;

	/** Travel speed through the tube, uu per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline", meta = (ClampMin = "10"))
	float FlowSpeed = 600.f;

	/** Packets in flight at once (at most MaxPacketSlots - the material has no more). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxPacketsInFlight = 8;

	UPROPERTY(ReplicatedUsing = OnRep_Packets, VisibleInstanceOnly, Category = "Logistics|Pipeline")
	TArray<FPipelinePacket> Packets;

	/** Seconds from building centre to building centre. Set by the server from the lane. */
	UPROPERTY(ReplicatedUsing = OnRep_Packets, VisibleInstanceOnly, BlueprintReadOnly, Category = "Logistics|Pipeline")
	float PacketTravelTime = 1.f;

	/** Server: room for another packet and PacketInterval since the last one. */
	bool CanLaunchPacket() const;

	/** Server: sends Amount of Type on its way (already taken from the source). */
	void LaunchPacket(EResourceType Type, float Amount);

	/** Server: removes the packets that reached the sink and returns them. */
	void TakeArrivedPackets(TArray<FPipelinePacket>& OutArrived);

	/** Server: what is on its way (of Type, or everything with MAX). */
	float GetInFlightAmount(EResourceType Type = EResourceType::MAX) const;

	// --- Look -----------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual")
	TObjectPtr<UMaterialInterface> PipeMaterial;

	/** Material of a supply pipeline (to a Converter). Empty = PipeMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual")
	TObjectPtr<UMaterialInterface> SupplyPipeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual", meta = (ClampMin = "5"))
	float TubeRadius = 40.f;

	/** Vertices around the tube. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual", meta = (ClampMin = "4", ClampMax = "32"))
	int32 TubeSides = 12;

	/** Distance between two rings of vertices - small enough for a round bulge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual", meta = (ClampMin = "5"))
	float RingSpacing = 25.f;

	/** Height of the tube's centre line above the ground; below TubeRadius = partly sunk in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual")
	float CenterHeight = 22.f;

	/**
	 * Sideways shift of the tube, to the right of its flow. A haul and a supply pipeline between the same two
	 * buildings flow in opposite directions - so they lie side by side instead of inside each other.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual")
	float LaneOffset = 55.f;

	/** At the building centres the tube dives this far below the ground, so it grows out of it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual", meta = (ClampMin = "0"))
	float EndSinkDepth = 80.f;

	/** Beyond this camera distance the wobble stops (0 = always on). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual", meta = (ClampMin = "0"))
	int32 WobbleDisableDistance = 0;

	// --- Roots: side shoots that grow from the tube into the ground, like a branch lying on the floor ---
	// Part of the same mesh (no extra draw call). Placed from a seed of the two building positions, so
	// every client builds the same ones.

	/** Average distance between two roots along the tube (0 = no roots). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "0"))
	float RootSpacing = 240.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "10"))
	float RootLengthMin = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "10"))
	float RootLengthMax = 320.f;

	/** Thickness where a root leaves the tube, relative to TubeRadius. It tapers to a point in the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "0.05", ClampMax = "1"))
	float RootRadiusScale = 0.42f;

	/**
	 * Length of the roots on the left of the flow - the side of a pipeline in the other direction (see
	 * LaneOffset) - so they do not grow through it. Only with a LaneOffset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "0", ClampMax = "1"))
	float InnerRootScale = 0.3f;

	/** Chance that a root forks once more. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "0", ClampMax = "1"))
	float RootForkChance = 0.55f;

	/** How deep the root tips reach below the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "0"))
	float RootTipDepth = 30.f;

	/** Vertices around a root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Roots", meta = (ClampMin = "3", ClampMax = "16"))
	int32 RootSides = 6;

	// --- Growth: a new pipeline grows from the source to the sink, roots after the tube has passed them ---

	/** Growth speed relative to the packets' speed. Above 1 the tube stays ahead of the first packet (0 = no growth). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Growth", meta = (ClampMin = "0"))
	float GrowSpeedFactor = 1.3f;

	/** Server world time the pipeline began to grow (< 0 = fully grown). Set once when the lane is set up. */
	UPROPERTY(ReplicatedUsing = OnRep_Packets, VisibleInstanceOnly, Category = "Logistics|Pipeline|Visual|Growth")
	double GrowStartTime = -1.0;

	/**
	 * Speed of pulling back (Retire) relative to the packets' speed. Below 1, so the packets flowing back stay
	 * ahead of the retreating tip (0 = removed at once).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Visual|Growth", meta = (ClampMin = "0", ClampMax = "1"))
	float RetractSpeedFactor = 0.7f;

	/** Server world times of pulling back (< 0 = not retiring). The server destroys the pipeline at RetractEndTime. */
	UPROPERTY(ReplicatedUsing = OnRep_Packets, VisibleInstanceOnly, Category = "Logistics|Pipeline|Visual|Growth")
	double RetractStartTime = -1.0;

	UPROPERTY(ReplicatedUsing = OnRep_Packets, VisibleInstanceOnly, Category = "Logistics|Pipeline|Visual|Growth")
	double RetractEndTime = -1.0;

	/** Pulls back into the sink (the source is gone) instead of into the source. */
	UPROPERTY(ReplicatedUsing = OnRep_Packets, VisibleInstanceOnly, Category = "Logistics|Pipeline|Visual|Growth")
	bool bRetractToSink = false;

	// --- Merging: pipelines of a team that run close together become one thick pipeline ----------------
	// The younger one bends onto the older one's centre line and ends inside it: where they run together only
	// the older one is drawn (thicker), so there is exactly one visible tube.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Merge")
	bool bMergeNearbyPipes = true;

	/** Centre lines closer than this (uu) count as running together. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Merge", meta = (ClampMin = "0"))
	float PipeMergeDistance = 160.f;

	/** Shortest stretch that is merged (uu) - a crossing is not. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Merge", meta = (ClampMin = "0"))
	float PipeMergeMinLength = 300.f;

	/** Length over which a pipeline bends onto the other one and away again (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Merge", meta = (ClampMin = "1"))
	float PipeMergeJoinLength = 300.f;

	/** Extra thickness per pipeline merged into this one (0.35 = +35 % radius). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Merge", meta = (ClampMin = "0"))
	float PipeMergeWidthPerPipe = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Merge", meta = (ClampMin = "1"))
	float PipeMergeMaxWidthScale = 1.8f;

	// --- Ground units walk over the tube ------------------------------------------------------------

	/**
	 * Invisible capsules along the tube that only answer WorldStatic object queries (the ground trace of the
	 * units): ground units walk over it. Navigation, camera and mouse traces ignore them.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Walkable")
	bool bWalkableByGroundUnits = true;

	/** Length of one collision capsule along the tube (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logistics|Pipeline|Walkable", meta = (ClampMin = "50"))
	float WalkCollisionSegmentLength = 250.f;

	/**
	 * Lays out every pipeline of TeamId (but Leaving): centre lines, merging, then rebuilds those that changed.
	 * Called whenever one of them changes or goes.
	 */
	static void SolvePipeNetwork(UWorld* World, int32 InTeamId, bool bInReplayRoads, const ALogisticsPipeline* Leaving);

protected:
	virtual void OnRoadInitialized() override;
	virtual void RebuildVisuals() override;
	virtual void ClearVisuals() override;

	UFUNCTION()
	void OnRep_Packets();

	/** Clients / listen server: writes the packets into the material. */
	void UpdatePacketParameters();

	/** The centre line from the source building's centre to the sink's, on the ground. */
	void BuildCenterLine(TArray<FVector>& OutPoints) const;

	/** The ground below P (ignores the two buildings and other roads / pipelines); P itself if there is none. */
	FVector TraceGround(const FVector& P) const;

	/** Builds the tube along Line with the radius TubeRadius * RadiusScale per ring. Merged per ring: 0 open,
	 *  1 lies on another pipeline (no roots), 2 inside another pipeline (not drawn). */
	void BuildTube(const TArray<FVector>& Line, const TArray<float>& RadiusScale, const TArray<uint8>& Merged);

	/** The walk capsules (bWalkableByGroundUnits) along Line. */
	void BuildWalkCollision(const TArray<FVector>& Line, const TArray<float>& RadiusScale);
	void ClearWalkCollision();

	/** Centre line of the own lane (before merging). */
	TArray<FVector> RawLine;

	/** What the tube was last built from - unchanged = no rebuild. */
	TArray<FVector> BuiltLine;
	TArray<float> BuiltScale;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UCapsuleComponent>> WalkCollision;

	double GetServerTime() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PipeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PipeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PipeMID;

	/** Length of the built tube; the material moves the packets along it in PacketTravelTime. */
	float VisualLength = 0.f;

	/** Longest UV0.x of the mesh (a root can reach beyond the tube's end) - the growth ends there. */
	float VisualGrowLength = 0.f;

	/** Smallest 2 * UV1.x - UV0.x of the mesh (root tips near the source) - pulling back into the sink starts there. */
	float VisualTailMin = 0.f;

	/** Server: destroys the pipeline once it has pulled back. */
	FTimerHandle RetractEndTimer;
	void OnRetractFinished();

	/** While growing the shadow follows the WPO; afterwards the wobble stays out of the shadow cache again. */
	FTimerHandle GrowEndTimer;
	void OnGrowFinished();

	/** Server. */
	double LastLaunchTime = -1.0e9;
};
