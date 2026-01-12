// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityUnit.h"
#include "NiagaraComponent.h"
#include "Actors/AreaDecalComponent.h"
#include "Actors/SelectionDecalComponent.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtr.h"

class UStaticMeshComponent;
class AStaticMeshActor;
class UMassActorBindingComponent;
class UInstancedStaticMeshComponent;
class USelectionDecalComponent;
class UNiagaraComponent;

#include "MassUnitBase.generated.h"

class AUnitBase;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AMassUnitBase : public AAbilityUnit
{
	GENERATED_BODY()

public:
	// Helper to apply/clear follow target from parent class
	static void ApplyFollowTargetForUnit(class AUnitBase* ThisUnit, class AUnitBase* NewFollowTarget);
	
	AMassUnitBase(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = ISM)
	virtual FVector GetMassActorLocation() const override;
	// The Mass Actor Binding Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ISM)
	UMassActorBindingComponent* MassActorBindingComponent;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = ISM)
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= ISM)
	int32 InstanceIndex = INDEX_NONE;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = ISM)
	bool bUseSkeletalMovement = true;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = ISM)
	bool bUseIsmWithActorMovement = true;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsFlying = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FlyHeight = 500.f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AddEffectTargetFragement = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AddGameplayEffectFragement = false;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USelectionDecalComponent* SelectionIcon;

	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	//UAreaDecalComponent* AreaDecalComponent;

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetInvisibility(bool NewInvisibility);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddStopMovementTagToEntity();

	// Apply or remove the tag that freezes only X/Y movement (allowing Z updates)
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool ApplyStopXYMovementTag(bool bApply);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool EnableDynamicObstacle(bool Enable);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetUnitAvoidanceEnabled(bool bEnable);

	// Enable or disable nav mesh manipulation for this unit by adding/removing FMassStateDisableNavManipulationTag
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetNavManipulationEnabled(bool bEnable);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveStopGameplayEffectTagToEntity();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddStopGameplayEffectTagToEntity();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveTagFromEntity(UScriptStruct* TagToRemove);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTag(UScriptStruct* TagToAdd);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTagByState(TEnumAsByte<UnitData::EState> UState, TEnumAsByte<UnitData::EState> UStatePlaceholder);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdateEntityStateOnUnload(const FVector& UnloadLocation);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool ResetTarget();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool EditUnitDetection(bool HasDetection);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool EditUnitDetectable(bool IsDetectable);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool FocusEntityTarget(AUnitBase* TargetUnit);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool IsUnitDetectable();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveFocusEntityTarget();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdateEntityHealth(float NewHealth);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SyncTranslation();

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SyncRotation();

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetTranslationLocation(FVector NewLocation);
	
	// Updates MoveTarget and ClientPrediction fragments to reflect a new location and desired speed
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdatePredictionFragment(const FVector& NewLocation, float DesiredSpeed = 0.f);

	// Stops Mass movement for this unit by setting MoveTarget to Stand on the server
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool StopMassMovement();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateISMInstanceTransform(int32 InstIndex, const FTransform& NewTransform);

	UFUNCTION(BlueprintCallable, Category = Mass)
	void StartAcceleratingTowardsDestination(const FVector& NewDestination, float NewAccelerationRate, float NewRequiredDistanceToStart);

	UFUNCTION(BlueprintCallable, Category = Mass)
	void StartCharge(const FVector& NewDestination, float ChargeSpeed, float ChargeDuration);
	
	bool GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_A;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FTransform Niagara_A_Start_Transform;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_B;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FTransform Niagara_B_Start_Transform;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FQuat MeshRotationOffset;
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	void InitializeUnitMode();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastTransformSync(const FVector& Location);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateISMLinear(const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent);
	
	// Rotate an arbitrary static mesh component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateActorLinear(UStaticMeshComponent* MeshToRotate, const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent);

	// Move the ISM instance or skeletal mesh smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastMoveISMLinear(const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent);

	// Move an arbitrary static mesh component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastMoveActorLinear(UStaticMeshComponent* MeshToMove, const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent);

	// Scale the ISM instance or skeletal mesh smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastScaleISMLinear(const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent);

 // Scale an arbitrary static mesh component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastScaleActorLinear(UStaticMeshComponent* MeshToScale, const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent);

	// Continuously pulsate the ISM/skeletal visual scale between Min and Max (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastPulsateISMScale(const FVector& InMinScale, const FVector& InMaxScale, float TimeMinToMax, bool bEnable);

	// Continuously rotate a static mesh component's Yaw to face UnitToChase (runs on server and clients)
	// bEnable starts/stops the continuous follow; YawOffsetDegrees is added to the facing yaw.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateActorYawToChase(UStaticMeshComponent* MeshToRotate, float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees);

	// Continuously rotate the whole unit's Yaw to face UnitToChase (runs on server and clients)
	// bEnable starts/stops the continuous follow; YawOffsetDegrees is added to the facing yaw.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateUnitYawToChase(float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees);

	// Continuously rotate the whole unit's Yaw at a constant rate (runs on server and clients)
	// YawRate is in degrees per second.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastContinuousUnitRotation(float YawRate, bool bEnable);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	// Smooth rotation controls (duration-based)
	
	float RotateDuration = 0.25f; // seconds to reach target; <=0 to snap
	float RotationEaseExponent = 1.0f; // 1 = linear; >1 to ease-in/out feel
	
	// Runtime state for smooth rotation (not replicated)
	FTimerHandle RotateTimerHandle;
	float RotateElapsed = 0.f;
	FQuat RotateStart;
	FQuat RotateTarget;
	
	// Per-frame rotation step while timer active
	void RotateISM_Step();
	
	// Helpers to read/apply current visual rotation
	FQuat GetCurrentLocalVisualRotation() const;
	void ApplyLocalVisualRotation(const FQuat& NewLocalRotation);

	// Smooth movement controls (duration-based)
	float MoveDuration = 0.25f; // seconds to reach target; <=0 to snap
	float MoveEaseExponent = 1.0f; // 1 = linear
	FTimerHandle MoveTimerHandle;
	float MoveElapsed = 0.f;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveTarget = FVector::ZeroVector;
	void MoveISM_Step();
	FVector GetCurrentLocalVisualLocation() const;
	void ApplyLocalVisualLocation(const FVector& NewLocalLocation);

	// Smooth scaling controls (duration-based)
	float ScaleDuration = 0.25f; // seconds to reach target; <=0 to snap
	float ScaleEaseExponent = 1.0f; // 1 = linear
	FTimerHandle ScaleTimerHandle;
	float ScaleElapsed = 0.f;
	FVector ScaleStart = FVector(1.f, 1.f, 1.f);
	FVector ScaleTarget = FVector(1.f, 1.f, 1.f);
	void ScaleISM_Step();
	FVector GetCurrentLocalVisualScale() const;
	void ApplyLocalVisualScale(const FVector& NewLocalScale);

	// Continuous pulsating scale state (not replicated)
	bool bPulsateScaleEnabled = false;
	FTimerHandle PulsateScaleTimerHandle;
	FVector PulsateMinScale = FVector(1.f, 1.f, 1.f);
	FVector PulsateMaxScale = FVector(1.f, 1.f, 1.f);
	float PulsateHalfPeriod = 1.0f; // seconds from Min to Max
	float PulsateElapsed = 0.f;
	void PulsateISMScale_Step();
	
	// Lightweight tween state for rotating arbitrary static mesh components
	struct FStaticMeshRotateTween
	{
		float Duration = 0.f;
		float Elapsed = 0.f;
		float EaseExp = 1.f;
		FQuat Start = FQuat::Identity;
		FQuat Target = FQuat::Identity;
	};
	
	// Timer step for rotating arbitrary static mesh components (state stored internally in cpp)
	void StaticMeshRotations_Step();
	FTimerHandle StaticMeshRotateTimerHandle;
	
	// Active tweens indexed by component; allows rotating multiple static meshes in parallel
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FStaticMeshRotateTween> ActiveStaticMeshTweens;

	// Continuous yaw-follow state per static mesh
	struct FYawFollowData
	{
		float Duration = 0.f;
		float EaseExp = 1.f;
		float OffsetDegrees = 0.f;
	};
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FYawFollowData> ActiveYawFollows;
	FTimerHandle StaticMeshYawFollowTimerHandle;
	void StaticMeshYawFollow_Step();

	// Continuous yaw-follow state for the unit itself
	FYawFollowData UnitYawFollowData;
	bool bUnitYawFollowEnabled = false;
	FTimerHandle UnitYawFollowTimerHandle;
	void UnitYawFollow_Step();

	// Smooth rotation tween for the unit itself
	FStaticMeshRotateTween UnitRotateTween;
	FTimerHandle UnitRotateTimerHandle;
	void UnitRotation_Step();

	// Continuous constant rotation state for the unit itself
	bool bContinuousUnitRotationEnabled = false;
	float ContinuousUnitYawRate = 0.f;
	FTimerHandle ContinuousUnitRotationTimerHandle;
	void ContinuousUnitRotation_Step();

	// Lightweight tween state for moving arbitrary static mesh components
	struct FStaticMeshMoveTween
	{
		float Duration = 0.f;
		float Elapsed = 0.f;
		float EaseExp = 1.f;
		FVector Start = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
	};
	void StaticMeshMoves_Step();
	FTimerHandle StaticMeshMoveTimerHandle;
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FStaticMeshMoveTween> ActiveStaticMeshMoveTweens;

	// Lightweight tween state for scaling arbitrary static mesh components
	struct FStaticMeshScaleTween
	{
		float Duration = 0.f;
		float Elapsed = 0.f;
		float EaseExp = 1.f;
		FVector Start = FVector(1.f, 1.f, 1.f);
		FVector Target = FVector(1.f, 1.f, 1.f);
	};
	void StaticMeshScales_Step();
	FTimerHandle StaticMeshScaleTimerHandle;
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FStaticMeshScaleTween> ActiveStaticMeshScaleTweens;
	
	//void InitializeUnitMode();
		
};
