// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/UnitBase.h"
#include "ConstructionUnit.generated.h"

class AWorkArea;
class UPrimitiveComponent;
class USceneComponent;
class UInstancedStaticMeshComponent;

UCLASS()
class RTSUNITTEMPLATE_API AConstructionUnit : public AUnitBase
{
	GENERATED_BODY()
public:
	AConstructionUnit(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetCharacterVisibility(bool desiredVisibility) override;

	// The worker assigned to construct this site
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	AUnitBase* Worker = nullptr;

	// Whether to scale Z to match WorkArea bounds when spawned
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	bool ScaleZ = false;

	// Whether the work/build area for this construction site
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	AWorkArea* WorkArea = nullptr;

	UPROPERTY(Replicated)
	FVector Rep_VE_OscillationOffsetA = FVector::ZeroVector;
	UPROPERTY(Replicated)
	FVector Rep_VE_OscillationOffsetB = FVector::ZeroVector;
	UPROPERTY(Replicated)
	float Rep_VE_OscillationCyclesPerSecond = 0.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	bool DroneBehavior = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	FVector2D DroneOrbitRadius = FVector2D(300.f, 300.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	FVector DroneOrbitCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneMeshSafetyBuffer = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneOrbitSpeed = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneRotationSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneInterpSpeed = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneAscentSpeed = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneSpawnHeight = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneSpawnPitch = 90.f;

	// Optional "DronePlane" projection ISM. When assigned (via SetDronePlaneISM) it follows the
	// drone as a rigid child — keeping its relative Blueprint offset — and is only shown during
	// the vertical-move stage (DroneStage == 3). Assign on EVERY machine (the reference is a local
	// component pointer, not replicated); the setter registers the visual on demand, so it is safe
	// from BeginPlay or the OnMassRegistrationFinished Blueprint event. Shows as None in the editor
	// — it is populated at runtime by SetDronePlaneISM.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction|Drone")
	UInstancedStaticMeshComponent* DronePlaneISM = nullptr;

	// Register an additional ISM as the "DronePlane" projection: stores the reference, ensures the
	// component participates in the Mass visual system, and propagates it to the visual fragment.
	UFUNCTION(BlueprintCallable, Category = "Construction|Drone")
	void SetDronePlaneISM(UInstancedStaticMeshComponent* InISM);

	UPROPERTY()
	int32 Rep_DroneStage = 0;

	UPROPERTY()
	float Rep_DroneTargetAngle = 0.f;

	UPROPERTY()
	float Rep_DroneTargetHeight = 0.f;

	UPROPERTY()
	float BuildingMaxHeight = 300.f;

	// Optional override for which component to animate (if null we try to auto-detect a mesh; fallback to RootComponent/Actor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
	UPrimitiveComponent* VisualComponentOverride = nullptr;

	// Defaults for rotation animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
	FVector DefaultRotateAxis = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
	float DefaultRotateDegreesPerSecond = 90.f;

 // Defaults for oscillation animation (relative local offsets)
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
 FVector DefaultOscOffsetA = FVector(0.f, 0.f, 0.f);

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
 FVector DefaultOscOffsetB = FVector(0.f, 0.f, 50.f);

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
 bool SetOffsetsDueToWorkAreaBounds = false;
	
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Anim")
 float DefaultOscillationCyclesPerSecond = 1.0f;

 // Pulsating scale configuration (applied multiplicatively on top of base scale from WorkArea fit)
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pulsate")
 bool bPulsateScaleDuringBuild = false;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pulsate")
 FVector PulsateMinMultiplier = FVector(0.9f, 0.9f, 0.9f);

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pulsate")
 FVector PulsateMaxMultiplier = FVector(1.1f, 1.1f, 1.1f);

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pulsate")
 float PulsateTimeMinToMax = 0.75f; // seconds from Min to Max

 // Start rotating the visual around an axis at DegreesPerSecond for Duration seconds (multicast to clients)
 UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = Construction)
 void MulticastStartRotateVisual(const FVector& Axis, float DegreesPerSecond, float Duration);

 // Move the visual back-and-forth between two local offsets for Duration seconds (multicast to clients)
 UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = Construction)
 void MulticastStartOscillateVisual(const FVector& LocalOffsetA, const FVector& LocalOffsetB, float CyclesPerSecond, float Duration);

 // Continuously pulsate actor/component scale around captured base (multicast to clients)
 UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "Construction|Pulsate")
 void MulticastPulsateScale(const FVector& MinMultiplier, const FVector& MaxMultiplier, float TimeMinToMax, bool bEnable);
	
 // Query if the pulsating scale is currently active on this client
 UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Construction|Pulsate")
 bool IsPulsatingScaleActive() const { return (Rep_VE_ActiveEffects & (1 << 0)) != 0; }

 // Utility: immediately mark this construction site as dead, zero its health, and hide it
 UFUNCTION(BlueprintCallable, Category = Construction)
 void KillConstructionUnit();

 // Server RPC backing for KillConstructionUnit
 UFUNCTION(Server, Reliable)
 void Server_KillConstructionUnit();

	protected:
	// Resolve which component we will animate
	UPrimitiveComponent* ResolveVisualComponent() const;
};
