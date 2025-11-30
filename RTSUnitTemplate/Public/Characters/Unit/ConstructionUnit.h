// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/UnitBase.h"
#include "ConstructionUnit.generated.h"

class AWorkArea;
class UPrimitiveComponent;
class USceneComponent;

UCLASS()
class RTSUNITTEMPLATE_API AConstructionUnit : public AUnitBase
{
	GENERATED_BODY()
public:
	AConstructionUnit(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The worker assigned to construct this site
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	AUnitBase* Worker = nullptr;

	// Whether to scale Z to match WorkArea bounds when spawned
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	bool ScaleZ = false;

	// The work/build area for this construction site
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	AWorkArea* WorkArea = nullptr;

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
	float DefaultOscillationCyclesPerSecond = 1.0f;

	// Start rotating the visual around an axis at DegreesPerSecond for Duration seconds (multicast to clients)
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = Construction)
	void MulticastStartRotateVisual(const FVector& Axis, float DegreesPerSecond, float Duration);

	// Move the visual back-and-forth between two local offsets for Duration seconds (multicast to clients)
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = Construction)
	void MulticastStartOscillateVisual(const FVector& LocalOffsetA, const FVector& LocalOffsetB, float CyclesPerSecond, float Duration);

	// Utility: immediately mark this construction site as dead, zero its health, and hide it
	UFUNCTION(BlueprintCallable, Category = Construction)
	void KillConstructionUnit();

	// Server RPC backing for KillConstructionUnit
	UFUNCTION(Server, Reliable)
	void Server_KillConstructionUnit();

protected:
	// Resolve which component we will animate
	UPrimitiveComponent* ResolveVisualComponent() const;

	// Timer steps
	void RotateVisual_Step();
	void OscillateVisual_Step();

	// Runtime state (not replicated)
	FTimerHandle RotateTimerHandle;
	float Rotate_Duration = 0.f;
	float Rotate_Elapsed = 0.f;
	FVector Rotate_Axis = FVector(0.f, 0.f, 0.f);
	float Rotate_DegreesPerSec = 0.f;
	TWeakObjectPtr<USceneComponent> Rotate_TargetComp;
	bool Rotate_UseActor = false;

	FTimerHandle OscillateTimerHandle;
	float Osc_Duration = 0.f;
	float Osc_Elapsed = 0.f;
	float Osc_CyclesPerSec = 1.f;
	FVector Osc_OffsetA = FVector::ZeroVector;
	FVector Osc_OffsetB = FVector(0.f, 0.f, 50.f);
	FVector Osc_BaseRelativeLoc = FVector::ZeroVector;
	FVector Osc_BaseActorLoc = FVector::ZeroVector;
	TWeakObjectPtr<USceneComponent> Osc_TargetComp;
	bool Osc_UseActor = false;
};
