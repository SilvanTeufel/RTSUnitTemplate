// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralPondComponent.generated.h"

class AProceduralPond;
class UMaterialParameterCollection;

/**
 * Procedural ponds for the game mode: at match start (StartPlay, after the procedural resources) it picks free,
 * flat spots on the navmesh and spawns an AProceduralPond on each. The water itself is drawn by the landscape
 * material (Static Switch "ProceduralPonds" + the Material Parameter Collection below); the pond blocks the
 * navmesh with the same ellipse, so units walk right up to the shore.
 *
 * Kept free: units and buildings, resources (the procedural ones when they are on - they have already removed
 * the placed ones by then - otherwise the placed ones), other work areas, player starts, waypoints and markers.
 * Independent of the procedural resources: switch it on and off with bEnabled.
 */
UCLASS(ClassGroup = (RTSUnitTemplate), meta = (BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UProceduralPondComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProceduralPondComponent();

	/** Ponds on/off for this game mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds")
	bool bEnabled = false;

	/**
	 * Not empty: ponds only on these maps (map name without path, e.g. Helix_Basin_AH). For a game mode that
	 * several maps share - the landscape material of each map has to draw the water (Static Switch
	 * "ProceduralPonds"), otherwise the ponds would only be bare, blocked ground.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds")
	TArray<FString> OnlyOnMaps;

	/** Must be the collection the landscape material reads (MPC_ProceduralPonds). Without it nothing is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds")
	TObjectPtr<UMaterialParameterCollection> PondCollection;

	/** Number of water bodies (ponds and rivers). Together they use at most 48 slots - a river takes one per segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "0", ClampMax = "48"))
	int32 MinPonds = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "0", ClampMax = "48"))
	int32 MaxPonds = 13;

	/** Half axes of a pond (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "100"))
	float MinRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "100"))
	float MaxRadius = 1600.f;

	/** > 1: mostly small ponds and now and then a big lake. 1 = sizes evenly spread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "0.2"))
	float SizeBias = 1.3f;

	/** Longest axis / shortest axis at most. 1 = only circles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "1"))
	float MaxElongation = 2.2f;

	/** Share of the water bodies that become rivers: a chain of overlapping, winding segments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "0", ClampMax = "1"))
	float RiverChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "2", ClampMax = "16"))
	int32 MinRiverSegments = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "2", ClampMax = "16"))
	int32 MaxRiverSegments = 12;

	/** Half width of a river (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "80"))
	float MinRiverWidth = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "80"))
	float MaxRiverWidth = 380.f;

	/** Half length of one river segment (uu). Segments overlap by about a third. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "200"))
	float RiverSegmentLength = 650.f;

	/** How much a river may turn from one segment to the next (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|River", meta = (ClampMin = "0", ClampMax = "60"))
	float RiverCurviness = 22.f;

	/** How far the material lowers the water below the shore (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "0"))
	float Depth = 25.f;

	/** Added to the water edge for the blocked navmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds", meta = (ClampMin = "0"))
	float NavPadding = 15.f;

	/** Free space between two ponds (shore to shore). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Clearance")
	float MinPondGap = 1100.f;

	/** Free space from the shore to units and buildings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Clearance")
	float UnitClearance = 400.f;

	/** Free space from the shore to resources and other work areas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Clearance")
	float ResourceClearance = 500.f;

	/** Free space from the shore to player starts (the base has to fit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Clearance")
	float PlayerStartClearance = 2500.f;

	/** Free space from the shore to waypoints (units spawn and gather there). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Clearance")
	float WaypointClearance = 700.f;

	/** Free space from the shore to story triggers (they often stand in the map as a visible object). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Clearance")
	float StoryTriggerClearance = 350.f;

	/** Ground under the pond and its shore may differ by at most this much (uu) - ponds only on flat land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Ground", meta = (ClampMin = "0"))
	float MaxHeightDifference = 60.f;

	/** Only on the landscape - not on rocks, ramps or other meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds|Ground")
	bool bLandscapeOnly = true;

	/** 0 = new ponds every match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ponds")
	int32 Seed = 0;

	/** Server: places the ponds. Called by ARTSGameModeBase::StartPlay. */
	UFUNCTION(BlueprintCallable, Category = "Ponds")
	void GeneratePonds();

	/** Removes the ponds of this match. */
	UFUNCTION(BlueprintCallable, Category = "Ponds")
	void ClearPonds();

	UFUNCTION(BlueprintPure, Category = "Ponds")
	TArray<AProceduralPond*> GetPonds() const { return TArray<AProceduralPond*>(Ponds); }

	/** True if the 2D point lies in a pond (grown by Margin). For later placements (buildings, spawns). */
	UFUNCTION(BlueprintPure, Category = "Ponds")
	bool IsInPond(const FVector& Location, float Margin = 0.f) const;

private:
	/** Some seconds after placing: is the navmesh really gone under every pond? Logs the result. */
	void CheckNavBlocked();
	FTimerHandle NavCheckTimer;
	int32 NavCheckRound = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProceduralPond>> Ponds;
};
