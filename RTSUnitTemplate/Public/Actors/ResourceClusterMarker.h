// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceClusterMarker.generated.h"

/** Which resources a cluster holds (see FProceduralSettings ...ClusterWeight). */
UENUM(BlueprintType)
enum class EResourceClusterMix : uint8
{
	/** By the ...ClusterWeight of the procedural settings. */
	Random            UMETA(DisplayName = "Random"),
	PrimaryOnly       UMETA(DisplayName = "Primary Only"),
	PrimarySecondary  UMETA(DisplayName = "Primary + Secondary"),
	PrimaryTertiary   UMETA(DisplayName = "Primary + Tertiary"),
	SecondaryTertiary UMETA(DisplayName = "Secondary + Tertiary"),
};

/** How the places of a cluster lie (see FProceduralSettings ...ShapeWeight). */
UENUM(BlueprintType)
enum class EResourceClusterShape : uint8
{
	/** By the ...ShapeWeight of the procedural settings. */
	Random   UMETA(DisplayName = "Random"),
	/** On a ring (3 = triangle, 4 = square); more than 6: one in the centre, the rest on a ring. */
	Ring     UMETA(DisplayName = "Ring"),
	/** A triangle: 3 corners, 4 = with one in the centre, 5-6 = with places on the edges. */
	Triangle UMETA(DisplayName = "Triangle"),
	/** In a (slightly bent) row. */
	Line     UMETA(DisplayName = "Line"),
};

/**
 * Place on a map: at game start a resource cluster always spawns here (AResourceGameMode, server only), with
 * the classes of the procedural settings (or of the map's resource places). Procedural clusters keep their
 * MinClusterDistance from it. Works with ProceduralSettings.Procedural off too - then only these clusters
 * are added and the map's resource places stay.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API AResourceClusterMarker : public AActor
{
	GENERATED_BODY()

public:
	AResourceClusterMarker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Cluster")
	EResourceClusterMix Mix = EResourceClusterMix::Random;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Cluster")
	EResourceClusterShape Shape = EResourceClusterShape::Random;

	/** Resource places of the cluster. 0 = the largest procedural size (ClusterSize). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Cluster", meta = (ClampMin = "0", ClampMax = "20"))
	int32 Size = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<class UBillboardComponent> Sprite;
#endif
};
