// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/FlyingBuildingAbility.h"
#include "Characters/Unit/UnitBase.h"

UFlyingBuildingAbility::UFlyingBuildingAbility()
{
	// A flying building must keep moving; do NOT stop it on activation.
	bStopMovementOnActivation = false;
}

AUnitBase* UFlyingBuildingAbility::GetFlyingBuildingAvatar() const
{
	return Cast<AUnitBase>(GetAvatarActorFromActorInfo());
}

void UFlyingBuildingAbility::TakeOff()
{
	if (AUnitBase* Unit = GetFlyingBuildingAvatar())
	{
		Unit->StartBuildingFlight(FlyHeight);
	}
}

void UFlyingBuildingAbility::FlyToLocationAndLand(FVector WorldLocation)
{
	if (AUnitBase* Unit = GetFlyingBuildingAvatar())
	{
		Unit->FlyUnitToLocationAndLand(WorldLocation, FlyHeight, MoveSpeed, AcceptanceRadius, DescendTime);
	}
}

void UFlyingBuildingAbility::LandNow()
{
	if (AUnitBase* Unit = GetFlyingBuildingAvatar())
	{
		// Begin the descent; the BP graph decides when to FinishLanding, or use FlyToLocationAndLand
		// for the automatic descend-then-freeze sequence.
		Unit->BeginLanding();
	}
}

void UFlyingBuildingAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Take off so the building hovers while the player aims the ability indicator.
	if (bTakeOffOnActivate)
	{
		TakeOff();
	}
}

void UFlyingBuildingAbility::OnAbilityMouseHit_Implementation(const FHitResult& InHitResult)
{
	FlyToLocationAndLand(InHitResult.Location);
}
