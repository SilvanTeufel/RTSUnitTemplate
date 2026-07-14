// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassClientBubbleInfoBase.h"
#include "Actors/Projectile.h"
#include "Mass/Replication/UnitReplicationPayload.h"
#include "UnitClientBubbleInfo.generated.h"

USTRUCT()
struct FProjectileStyle
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<class AProjectile> ProjectileClass;

	UPROPERTY()
	float Speed = 0.f;

	UPROPERTY()
	FVector Scale = FVector::OneVector;

	UPROPERTY()
	float Spread = 0.f;

	UPROPERTY()
	int32 ProjectileCount = 1;

	UPROPERTY()
	int32 MaxPiercedTargets = 1;

	UPROPERTY()
	bool IsBouncingNext = false;

	UPROPERTY()
	bool IsBouncingBack = false;

	UPROPERTY()
	float ZOffset = 0.f;

	UPROPERTY()
	FVector SpawnOffset = FVector::ZeroVector;

	UPROPERTY()
	bool DisableAutoZOffset = false;

	UPROPERTY()
	float TwinProjectileDistance = 0.f;

	// Homing parameters
	UPROPERTY()
	float HomingInitialAngle = 0.f;

	UPROPERTY()
	float HomingRotationSpeed = 0.f;

	UPROPERTY()
	float HomingMaxSpiralRadius = 0.f;

	UPROPERTY()
	float HomingInterpSpeed = 0.f;

	UPROPERTY()
	bool bFollowTarget = false;

	bool IsSameAs(const FProjectileStyle& Other) const
	{
		return ProjectileClass == Other.ProjectileClass &&
			FMath::IsNearlyEqual(Speed, Other.Speed) &&
			Scale.Equals(Other.Scale, 0.05f) &&
			FMath::IsNearlyEqual(Spread, Other.Spread, 0.1f) &&
			ProjectileCount == Other.ProjectileCount &&
			MaxPiercedTargets == Other.MaxPiercedTargets &&
			IsBouncingNext == Other.IsBouncingNext &&
			IsBouncingBack == Other.IsBouncingBack &&
			FMath::IsNearlyEqual(ZOffset, Other.ZOffset, 0.1f) &&
			SpawnOffset.Equals(Other.SpawnOffset, 0.1f) &&
			DisableAutoZOffset == Other.DisableAutoZOffset &&
			FMath::IsNearlyEqual(TwinProjectileDistance, Other.TwinProjectileDistance, 0.1f) &&
			FMath::IsNearlyEqual(HomingInitialAngle, Other.HomingInitialAngle, 0.1f) &&
			FMath::IsNearlyEqual(HomingRotationSpeed, Other.HomingRotationSpeed, 0.1f) &&
			FMath::IsNearlyEqual(HomingMaxSpiralRadius, Other.HomingMaxSpiralRadius, 0.1f) &&
			FMath::IsNearlyEqual(HomingInterpSpeed, Other.HomingInterpSpeed, 0.1f) &&
			bFollowTarget == Other.bFollowTarget;
	}
};

USTRUCT()
struct FPlayerMouseData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerId = -1;

	UPROPERTY()
	FVector_NetQuantize MouseLocation = FVector::ZeroVector;

	FPlayerMouseData() {}
	FPlayerMouseData(int32 InPlayerId, const FVector& InLocation)
		: PlayerId(InPlayerId), MouseLocation(InLocation) {}

	bool operator==(const FPlayerMouseData& Other) const
	{
		return PlayerId == Other.PlayerId && MouseLocation.Equals(Other.MouseLocation, 1.0f);
	}

	bool operator!=(const FPlayerMouseData& Other) const
	{
		return !(*this == Other);
	}
};

UCLASS()
class RTSUNITTEMPLATE_API AUnitClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()
public:
	AUnitClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Die replizierten Agenten für diese Bubble
	UPROPERTY(ReplicatedUsing=OnRep_Agents)
	FUnitReplicationArray Agents;

	UPROPERTY(Replicated)
	TArray<FProjectileStyle> ProjectileStyleRegistry;

	UPROPERTY(Replicated)
	TArray<FPlayerMouseData> PlayerMouseDatas;

	uint8 GetOrCreateStyleIndex(const FProjectileStyle& InStyle);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Agents();

protected:
	virtual void BeginPlay() override;
};
