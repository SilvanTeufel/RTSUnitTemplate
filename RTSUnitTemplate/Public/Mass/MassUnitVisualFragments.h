// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassUnitVisualFragments.generated.h"

USTRUCT()
struct RTSUNITTEMPLATE_API FMassVisualTweenState
{
    GENERATED_BODY()

    UPROPERTY()
    FQuat StartRotation = FQuat::Identity;

    UPROPERTY()
    FQuat TargetRotation = FQuat::Identity;

    UPROPERTY()
    FVector StartLocation = FVector::ZeroVector;

    UPROPERTY()
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY()
    FVector StartScale = FVector::OneVector;

    UPROPERTY()
    FVector TargetScale = FVector::OneVector;

    UPROPERTY()
    float Duration = 0.f;

    UPROPERTY()
    float Elapsed = 0.f;

    UPROPERTY()
    float EaseExp = 1.f;

    UPROPERTY()
    bool bActive = false;

    UPROPERTY()
    bool bTeleport = false;

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> TargetISM;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FMassVisualTweenFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    FMassVisualTweenState RotationTween;

    UPROPERTY()
    FMassVisualTweenState LocationTween;

    UPROPERTY()
    FMassVisualTweenState ScaleTween;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FMassUnitVisualInstance
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> TemplateISM;

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> TargetISM;

    UPROPERTY()
    int32 InstanceIndex = INDEX_NONE;

    // Offset from the actor's transform (from BP)
    UPROPERTY()
    FTransform BaseOffset = FTransform::Identity;

    // Current relative transform (BaseOffset + Tweens)
    UPROPERTY()
    FTransform CurrentRelativeTransform = FTransform::Identity;

    UPROPERTY()
    bool bWasVisible = false;

    // Relativer Transform von ProjectileSpawn zu dieser TemplateISM
    UPROPERTY()
    FTransform MuzzleOffset = FTransform::Identity;

    UPROPERTY()
    bool bHasMuzzle = false;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FMassUnitVisualFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FMassUnitVisualInstance> VisualInstances;

    UPROPERTY()
    bool bUseSkeletalMovement = false;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FMassVisualEffectFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    bool bForceHidden = false;

    // Pulsate Effect
    UPROPERTY()
    bool bPulsateEnabled = false;

    UPROPERTY()
    FVector PulsateMinScale = FVector::OneVector;

    UPROPERTY()
    FVector PulsateMaxScale = FVector::OneVector;

    UPROPERTY()
    float PulsateHalfPeriod = 1.f;

    UPROPERTY()
    float PulsateElapsed = 0.f;

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> PulsateTargetISM;

    // Yaw Chase Effect
    UPROPERTY()
    bool bYawChaseEnabled = false;

    UPROPERTY()
    FMassEntityHandle TargetEntity;

    UPROPERTY()
    float YawChaseOffset = 0.f;

    UPROPERTY()
    float YawChaseDuration = 0.2f;

    UPROPERTY()
    float YawChaseEaseExp = 1.f;

    UPROPERTY()
    bool bYawChaseTeleport = false;

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> YawChaseTargetISM;

    // Continuous Rotation
    UPROPERTY()
    bool bRotationEnabled = false;

    UPROPERTY()
    FVector RotationAxis = FVector::UpVector;

    UPROPERTY()
    float RotationDegreesPerSecond = 0.f;

    UPROPERTY()
    float RotationDuration = -1.f; // Negative for infinite

    UPROPERTY()
    float RotationElapsed = 0.f;

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> RotationTargetISM;

    // Oscillation
    UPROPERTY()
    bool bOscillationEnabled = false;

    UPROPERTY()
    FVector OscillationOffsetA = FVector::ZeroVector;

    UPROPERTY()
    FVector OscillationOffsetB = FVector::ZeroVector;

    UPROPERTY()
    float OscillationCyclesPerSecond = 1.f;

    UPROPERTY()
    float OscillationDuration = -1.f; // Negative for infinite

    UPROPERTY()
    float OscillationElapsed = 0.f;

    UPROPERTY()
    TWeakObjectPtr<UInstancedStaticMeshComponent> OscillationTargetISM;
};

template<>
struct TMassFragmentTraits<FMassUnitVisualFragment>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true,
	};
};
