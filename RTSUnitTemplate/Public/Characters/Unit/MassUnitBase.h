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

	UFUNCTION(BlueprintCallable, Category = ISM)
	virtual FRotator GetMassActorRotation() const override;

	UFUNCTION(BlueprintCallable, Category = ISM)
	virtual FTransform GetMassActorTransform() const override;
	// The Mass Actor Binding Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ISM)
	UMassActorBindingComponent* MassActorBindingComponent;

	UFUNCTION(BlueprintImplementableEvent, Category = "Mass")
	void OnMassRegistrationFinished();
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = ISM)
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ISM")
	TArray<UInstancedStaticMeshComponent*> AdditionalISMComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= ISM)
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(Transient)
	bool bMassVisualsRegistered = false;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bIsMassUnit = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = ISM)
	bool bUseSkeletalMovement = true;

	//UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = ISM)
	//bool bUseIsmWithActorMovement = true;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsFlying = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FlyHeight = 500.f;

	// --- Visual Effect Replication ---
	UPROPERTY(Replicated)
	uint8 Rep_VE_ActiveEffects = 0;

	UPROPERTY(Replicated)
	FVector Rep_VE_PulsateMinScale = FVector::OneVector;
	UPROPERTY(Replicated)
	FVector Rep_VE_PulsateMaxScale = FVector::OneVector;
	UPROPERTY(Replicated)
	float Rep_VE_PulsateHalfPeriod = 0.f;

	UPROPERTY(Replicated)
	FVector Rep_VE_RotationAxis = FVector::UpVector;
	UPROPERTY(Replicated)
	float Rep_VE_RotationDegreesPerSecond = 0.f;

	UPROPERTY(Replicated)
	FVector Rep_VE_OscillationOffsetA = FVector::ZeroVector;
	UPROPERTY(Replicated)
	FVector Rep_VE_OscillationOffsetB = FVector::ZeroVector;
	UPROPERTY(Replicated)
	float Rep_VE_OscillationCyclesPerSecond = 0.f;

	UPROPERTY(Replicated)
	float Rep_VE_DishSpeedMin = 0.f;

	UPROPERTY(Replicated)
	float Rep_VE_DishSpeedMax = 0.f;

	UPROPERTY(Replicated)
	float Rep_VE_DishDurationMin = 0.f;

	UPROPERTY(Replicated)
	float Rep_VE_DishDurationMax = 0.f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AddEffectTargetFragement = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AddGameplayEffectFragement = false;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USelectionDecalComponent* SelectionIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HealthCompCreated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bForceWidgetPosition = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int HideHealthBarUnitCount = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool TimerUpdateTriggered = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* HealthWidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float HealthWidgetHeightOffset = 150.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector HealthWidgetRelativeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UWidgetComponent* TimerWidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TimerWidgetHeightOffset = 100.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector TimerWidgetRelativeOffset;

protected:
	// Cached widget pointers to avoid repeated casting every tick
	UPROPERTY(Transient)
	class UUnitBaseHealthBar* CachedHealthBarWidget = nullptr;

	UPROPERTY(Transient)
	class UUnitTimerWidget* CachedTimerWidget = nullptr;

public:
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	//UAreaDecalComponent* AreaDecalComponent;

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetInvisibility(bool NewInvisibility);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddStopMovementTagToEntity();

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddStopSeparationTagToEntity();

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
	bool UpdateEntityHealth(float NewHealth, float CurrentShield = -1.f);
	
	virtual bool UpdateLevelUpTimestamp() override;
	
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
	
	// Fill or clear the path fragment with a list of world-space waypoints. If bClearExistingFirst is true, it resets the queue first.
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetPathWaypoints(const TArray<FVector>& NewPoints, bool bClearExistingFirst, bool bAttackToggled = false);
	
	// Clears all queued waypoints in the path fragment.
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool ClearPathWaypoints();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateISMInstanceTransform(int32 InstIndex, const FTransform& NewTransform);

	UFUNCTION(BlueprintCallable, Category = Mass)
	void StartAcceleratingTowardsDestination(const FVector& NewDestination, float NewAccelerationRate, float NewRequiredDistanceToStart);

	UFUNCTION(BlueprintCallable, Category = Mass)
	void StartCharge(const FVector& NewDestination, float ChargeSpeed, float ChargeDuration);
	
	bool GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle);
	bool GetMassEntityData(const FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle) const;

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
	
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = Mass)
	void InitializeUnitMode();

	bool RegisterVisualsToMass();
	bool RegisterAdditionalVisualsToMass();

	UFUNCTION(BlueprintCallable, Category = "Mass|Visual")
	void RemoveAdditionalISMInstances();

	UFUNCTION(BlueprintCallable, Category = Mass)
	void InitializeAdditionalISM(UInstancedStaticMeshComponent* InISMComponent);

	/** Returns the actual ISM component and instance index used by the Mass Visual Manager. 
	 * Matches the TemplateISM (e.g. ISMComponent or an Additional ISM) to the manager's instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass|Visual")
	bool GetMassVisualInstance(UInstancedStaticMeshComponent* TemplateISM, UInstancedStaticMeshComponent*& OutComponent, int32& OutInstanceIndex) const;

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastTransformSync(const FVector& Location);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateISMLinear(const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent, UInstancedStaticMeshComponent* InISMComponent = nullptr);
	
	// Rotate an arbitrary static mesh component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateActorLinear(UStaticMeshComponent* MeshToRotate, const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent);

	// Rotate an arbitrary niagara component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateNiagaraLinear(UNiagaraComponent* NiagaraToRotate, const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent);

	// Move the ISM instance or skeletal mesh smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastMoveISMLinear(const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent, UInstancedStaticMeshComponent* InISMComponent = nullptr);

	// Move an arbitrary static mesh component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastMoveActorLinear(UStaticMeshComponent* MeshToMove, const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent);

	// Scale the ISM instance or skeletal mesh smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastScaleISMLinear(const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent, UInstancedStaticMeshComponent* InISMComponent = nullptr);

	// Scale an arbitrary static mesh component smoothly over time (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastScaleActorLinear(UStaticMeshComponent* MeshToScale, const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent);

	// Continuously pulsate the ISM/skeletal visual scale between Min and Max (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastPulsateISMScale(const FVector& InMinScale, const FVector& InMaxScale, float TimeMinToMax, bool bEnable, UInstancedStaticMeshComponent* InISMComponent = nullptr);

	// Continuously rotate a static mesh component's Yaw to face UnitToChase (runs on server and clients)
	// bEnable starts/stops the continuous follow; YawOffsetDegrees is added to the facing yaw.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateActorYawToChase(UStaticMeshComponent* MeshToRotate, float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees);

	// Continuously rotate an ISM instance's Yaw to face UnitToChase (runs on server and clients)
	// bEnable starts/stops the continuous follow; YawOffsetDegrees is added to the facing yaw.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateISMYawToChase(UInstancedStaticMeshComponent* ISMToRotate, float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees, bool bTeleport);

	// Continuously rotate the whole unit's Yaw to face UnitToChase (runs on server and clients)
	// bEnable starts/stops the continuous follow; YawOffsetDegrees is added to the facing yaw.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastRotateUnitYawToChase(float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees);

	// Continuously rotate the whole unit's Yaw at a constant rate (runs on server and clients)
	// YawRate is in degrees per second.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastContinuousUnitRotation(float YawRate, bool bEnable);

	// Smoothly move the whole unit (Actor) by a relative offset (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastMoveUnitLinear(const FVector& RelativeLocationChange, float InMoveDuration, float InMoveEaseExponent);

	// Continuously rotate an ISM instance's Yaw at a constant rate (runs on server and clients)
	// YawRate is in degrees per second. Duration <= 0 means infinite.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastContinuousISMRotation(float YawRate, bool bEnable, float Duration = -1.f, UInstancedStaticMeshComponent* InISMComponent = nullptr);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastEndlessISMYawRotation(UInstancedStaticMeshComponent* InISMComponent, float YawRate, bool bEnable);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastDishISMYawRotation(UInstancedStaticMeshComponent* InISMComponent, float MinSpeed, float MaxSpeed, float MinDur, float MaxDur, bool bEnable);

	// Smoothly move an ISM instance by a relative offset (runs on server and clients)
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastMoveISMLinearRelative(const FVector& RelativeLocationChange, float InMoveDuration, float InMoveEaseExponent, UInstancedStaticMeshComponent* InISMComponent = nullptr);

	// Instantly sync the transform of an ISM instance across the network
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = RTSUnitTemplate)
	void MulticastISMTransformSync(const FVector& Location, const FRotator& Rotation, const FVector& Scale, UInstancedStaticMeshComponent* InISMComponent = nullptr);

	UFUNCTION(BlueprintImplementableEvent, Category = "RTS|Hover")
	void CustomOverlapStart(int32 InInstanceIndex, USkeletalMeshComponent* InMesh);

	UFUNCTION(BlueprintImplementableEvent, Category = "RTS|Hover")
	void CustomOverlapEnd(int32 InInstanceIndex, USkeletalMeshComponent* InMesh);

	UFUNCTION(BlueprintCallable, Category = "RTS|Mass")
	bool EditHoverTag(bool bApply);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	FQuat GetCurrentLocalVisualRotation(UInstancedStaticMeshComponent* InISM = nullptr) const;

	FVector GetCurrentLocalVisualLocation(UInstancedStaticMeshComponent* InISM = nullptr) const;

	FVector GetCurrentLocalVisualScale(UInstancedStaticMeshComponent* InISM = nullptr) const;

	// Fragment helpers
public:
	struct FMassUnitVisualFragment* GetMutableVisualFragment();
	const struct FMassUnitVisualFragment* GetVisualFragment() const;
protected:
	struct FMassVisualTweenFragment* GetMutableTweenFragment();
	struct FMassVisualEffectFragment* GetMutableEffectFragment();
	
	// Lightweight tween state for rotating arbitrary static mesh components
	struct FStaticMeshRotateTween
	{
		float Duration = 0.f;
		float Elapsed = 0.f;
		float EaseExp = 1.f;
		FQuat Start = FQuat::Identity;
		FQuat Target = FQuat::Identity;
		bool bTeleport = true;
	};
	
	// Timer step for rotating arbitrary static mesh components (state stored internally in cpp)
	void StaticMeshRotations_Step();
	FTimerHandle StaticMeshRotateTimerHandle;
	
	// Active tweens indexed by component; allows rotating multiple static meshes in parallel
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FStaticMeshRotateTween> ActiveStaticMeshTweens;

	// Timer step for rotating arbitrary niagara components
	void NiagaraRotations_Step();
	FTimerHandle NiagaraRotateTimerHandle;

	// Active tweens indexed by component; allows rotating multiple niagara components in parallel
	TMap<TWeakObjectPtr<UNiagaraComponent>, FStaticMeshRotateTween> ActiveNiagaraTweens;

	// Continuous yaw-follow state per static mesh
	struct FYawFollowData
	{
		float Duration = 0.f;
		float EaseExp = 1.f;
		float OffsetDegrees = 0.f;
		bool bTeleport = true;
	};
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FYawFollowData> ActiveYawFollows;
	FTimerHandle StaticMeshYawFollowTimerHandle;
	void StaticMeshYawFollow_Step();

	// Lightweight tween state for moving arbitrary static mesh components
	struct FStaticMeshMoveTween
	{
		float Duration = 0.f;
		float Elapsed = 0.f;
		float EaseExp = 1.f;
		FVector Start = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
	};

	// Smooth movement tween for the unit itself
	FStaticMeshMoveTween UnitMoveTween;
	FTimerHandle UnitMoveTimerHandle;
	void UnitMove_Step();

	// Continuous constant rotation state for the unit itself
	bool bContinuousUnitRotationEnabled = false;
	float ContinuousUnitYawRate = 0.f;
	FTimerHandle ContinuousUnitRotationTimerHandle;
	void ContinuousUnitRotation_Step();

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
		
};
