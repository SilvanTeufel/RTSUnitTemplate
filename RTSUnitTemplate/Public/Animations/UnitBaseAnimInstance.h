// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Core/UnitData.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Sound\SoundCue.h"
#include "UnitBaseAnimInstance.generated.h"

class AUnitBase;

USTRUCT(BlueprintType)
struct FUnitLocomotionBlendData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Speed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DirectionDegrees = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIsMoving = false;
};

USTRUCT(BlueprintType)
struct FUnitAnimData : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> AnimState = UnitData::Idle;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* Sound = nullptr;
	
};

/**
 *
 */
UCLASS(transient, Blueprintable, hideCategories = AnimInstance, BlueprintType)
class RTSUNITTEMPLATE_API UUnitBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UUnitBaseAnimInstance();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bUseLocomotionBlendspaceInputs = true;

	/** Speed must exceed this to trigger bIsMoving (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionIdleSpeedThreshold = 10.0f;

	/** How fast the smoothed speed ramps up (higher = snappier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionSpeedInterpUp = 12.0f;

	/** How fast the smoothed speed ramps down to idle (lower = slower fade-out) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionSpeedInterpDown = 6.0f;

	/** How fast direction smooths (higher = snappier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionDirectionInterp = 10.0f;

	/** Enter moving when filtered speed rises above this (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionMoveStartSpeed = 45.0f;

	/** Stay moving until filtered speed drops below this (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionMoveStopSpeed = 20.0f;

	/** Hold last reliable velocity briefly when inputs momentarily drop to zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionVelocityHoldTime = 0.12f;

	/** Ignore estimated spikes above this speed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionMaxEstimatedSpeed = 2200.0f;

	/** Minimum 2D step before delta-location velocity is considered reliable (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate, meta=(ClampMin="0.0"))
	float LocomotionMinEstimatedStep = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FUnitLocomotionBlendData LocomotionData;

	/** Smoothed speed fed to the blendspace (use this as your Speed axis input) */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float SmoothedSpeed = 0.0f;

	/** Smoothed direction fed to the blendspace (use this as your Direction axis input) */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float SmoothedDirection = 0.0f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TEnumAsByte<UnitData::EState> CharAnimState;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TEnumAsByte<UnitData::EState> LastAnimState = UnitData::None;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_1 = 0;
	
	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BlendPoint_2 = 0;

	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CurrentBlendPoint_1 = 0;
	
	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float CurrentBlendPoint_2 = 0;
	
	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_1 = 0.5;

	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TransitionRate_2 = 0.5;

	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_1 = 0;

	UPROPERTY(Replicated,VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Resolution_2 = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* Sound;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float SoundTimer = 0.f;
/*
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ControlTimer = 0.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UpdateTime = 0.1f;
	*/
	UFUNCTION()
	virtual void NativeInitializeAnimation() override;
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UFUNCTION()
	virtual void NativeUpdateAnimation(float Deltaseconds) override;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UDataTable* AnimDataTable;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBlendPoints(AUnitBase* Unit, float Deltaseconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateLocomotionData(const AUnitBase* Unit, float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	bool IsLocomotionState(TEnumAsByte<UnitData::EState> State) const;

	FUnitAnimData* UnitAnimData;

private:
	FVector LastActorLocation = FVector::ZeroVector;
	bool bHasLastActorLocation = false;

	/** Track if we're currently in a locomotion state to avoid resetting smoothed values mid-movement */
	bool bWasInLocomotionLastFrame = false;

	/** Track moving edge for start snap in anim smoothing */
	bool bWasMovingLastFrame = false;

	/** Runtime filtered planar velocity used to stabilize locomotion input */
	FVector2D FilteredVelocity2D = FVector2D::ZeroVector;
	FVector2D LastReliableVelocity2D = FVector2D::ZeroVector;
	float TimeSinceReliableVelocity = 0.0f;
	bool bIsReliableMoving = false;
	
};