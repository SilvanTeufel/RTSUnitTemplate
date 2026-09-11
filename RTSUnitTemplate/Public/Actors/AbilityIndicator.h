// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"

// Forward declarations for properties used below
class UMaterialInterface;
class UStaticMeshComponent;
class AGASUnit;

// NOTE: generated.h must remain the last include in this header
#include "AbilityIndicator.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AAbilityIndicator : public AActor
{
	GENERATED_BODY()
	
public: 
	// Sets default values for this actor's properties
	AAbilityIndicator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Sets the position of the indicator
	void SetIndicatorLocation(const FVector& NewLocation);

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	int TeamId = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSUnitTemplate")
	UStaticMeshComponent* IndicatorMesh;

	/** Nur die LANDSCHAFT zaehlt als Zielflaeche: der Marker wird vom getroffenen Punkt senkrecht
	 *  auf das Gelaende heruntergezogen, statt auf Einheiten und Gebaeude zu klettern.
	 *
	 *  Bewusst HIER und nicht nur an der Faehigkeit: der Marker wird lokal gespawnt, seine
	 *  Einstellungen stehen auf dem Client also sofort bereit. Die Faehigkeit erreicht den Client
	 *  erst ueber CurrentSnapshot - bis dahin waere die Regel dort wirkungslos. Steht das Flag
	 *  hier auf false, gilt weiterhin die Einstellung der Faehigkeit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bTargetLandscapeOnly = false;

	/** Kanal fuer die Zielsuche. Nur wirksam, wenn bTargetLandscapeOnly hier gesetzt ist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// If true, the controller will detect overlaps with WorkAreas and highlight this indicator
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool DetectOverlapWithWorkArea = false;

	// Cached original material of the indicator; set at BeginPlay if not assigned
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	UMaterialInterface* OriginalMaterial = nullptr;

	// Temporary highlight material used while overlapping a WorkArea
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	UMaterialInterface* TemporaryHighlightMaterial = nullptr;

	// True while the indicator is overlapping a NoBuildZone (controls highlight/validation)
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool IsOverlappedWithNoBuildZone = false;

	// Placement constraint: when true, this indicator cannot be placed closer than ResourcePlacementDistance to any resource WorkArea
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool DenyPlacementCloseToResources = false;

	// Minimum center-to-center distance to resource WorkAreas when PlacementCloseToResources is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float ResourcePlacementDistance = 1000.f;

	// If true, the indicator rotates to face from its owning unit/building toward the indicator location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool RotatesToDirection = false;

	// If true, this indicator will ignore its owning unit/building in distance/overlap checks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bIgnoreHoldingUnitInDistanceCheck = true;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate")
	TArray<AGASUnit*> AssociatedUnits;

private:
	// Root Scene Component
	UPROPERTY(VisibleAnywhere, Category = "RTSUnitTemplate")
	USceneComponent* SceneRoot;
	
};



