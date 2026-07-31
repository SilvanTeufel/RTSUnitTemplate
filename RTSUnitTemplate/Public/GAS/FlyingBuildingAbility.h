// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "FlyingBuildingAbility.generated.h"

class AUnitBase;

/**
 * Example child ability that makes the owning BUILDING (the ability avatar) take off, fly to the
 * clicked AbilityIndicator location, and land there automatically.
 *
 * Designed to be subclassed in Blueprint: set AbilityIndicatorClass (a ground indicator) so a
 * mouse-follow marker appears on activation, then either use the default behaviour (take off on
 * activate + fly-and-land on OnAbilityMouseHit) or compose your own graph with the BlueprintCallable
 * wrappers below (TakeOff / FlyToLocationAndLand / LandNow) plus the AUnitBase flight building blocks
 * (StartBuildingFlight, MoveUnitToLocation, BeginLanding, FinishLanding, IsUnitAtLocation2D).
 */
UCLASS()
class RTSUNITTEMPLATE_API UFlyingBuildingAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UFlyingBuildingAbility();

	// Hover height above ground while flying.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Flight")
	float FlyHeight = 500.f;

	// Horizontal move speed while flying to the target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Flight")
	float MoveSpeed = 600.f;

	// How close (X/Y) the building must get to the target before it starts landing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Flight")
	float AcceptanceRadius = 60.f;

	// Seconds allowed for the descent before the building is re-frozen on the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Flight")
	float DescendTime = 2.5f;

	// If true, the building takes off automatically when the ability activates (so it hovers while
	// the player aims the indicator). Turn off to drive take-off yourself from the BP graph.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Flight")
	bool bTakeOffOnActivate = true;

	// The owning building (ability avatar) as an AUnitBase. Usable anywhere in the BP graph.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTSUnitTemplate|Flight")
	AUnitBase* GetFlyingBuildingAvatar() const;

	// --- Composable wrappers (act on the avatar building) ---
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void TakeOff();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void FlyToLocationAndLand(FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Flight")
	void LandNow();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// Default: fly to the confirmed click location and auto-land. BP subclasses may override.
	virtual void OnAbilityMouseHit_Implementation(const FHitResult& InHitResult) override;
};
