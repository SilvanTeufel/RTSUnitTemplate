// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/SupplyBoostAbility.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameModes/ResourceGameMode.h"

USupplyBoostAbility::USupplyBoostAbility()
{
	// A cancelled cast gives the price back (UGameplayAbilityBase::EndAbility).
	bRefundOnCancel = true;
}

bool USupplyBoostAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const ABuildingBase* Building = ActorInfo ? Cast<ABuildingBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	// SupplyBoostCount is replicated, so a client refuses a second use as well (e.g. a queued double click).
	return Building && (!bDisableAfterUse || Building->SupplyBoostCount == 0);
}

void USupplyBoostAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ABuildingBase* Building = ActorInfo ? Cast<ABuildingBase>(ActorInfo->AvatarActor.Get()) : nullptr;

	// With a cast the base class enters and leaves the Casting state and the casting processor reports the end
	// through HandleCastComplete (as ULogisticsSummonAbility). Read at activation, so it is set before Super.
	bUseCastingFallbackProcessor = BoostCastTime > 0.f;

	AResourceGameMode* GameMode = Building ? Cast<AResourceGameMode>(Building->GetWorld()->GetAuthGameMode()) : nullptr;
	const bool bHasCost = ConstructionCost.PrimaryCost || ConstructionCost.SecondaryCost || ConstructionCost.TertiaryCost
		|| ConstructionCost.RareCost || ConstructionCost.EpicCost || ConstructionCost.LegendaryCost;

	if (bUseCastingFallbackProcessor && Building && Building->HasAuthority())
	{
		// Pay first; not affordable = nothing happens (ended before Super, so no Casting state either).
		if (bHasCost && (!GameMode || !GameMode->ModifyResourceCCost(ConstructionCost, Building->TeamId)))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
			return;
		}
		Building->SetTimerWidgetCastingColor(CastTimerColor);
		Building->UnitControlTimer = 0.f;
		Building->CastTime = BoostCastTime;
		if (CastSound)
		{
			Building->FireEffects(nullptr, CastSound, FVector::OneVector, SoundVolume);
		}
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!Building || !Building->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
		return;
	}

	// Pay first; not affordable = nothing happens.
	if (bHasCost && (!GameMode || !GameMode->ModifyResourceCCost(ConstructionCost, Building->TeamId)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
		return;
	}

	GrantBoost(Building);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
}

void USupplyBoostAbility::HandleCastComplete(const FHitResult& InHitResult)
{
	Super::HandleCastComplete(InHitResult);

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ABuildingBase* Building = ActorInfo ? Cast<ABuildingBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsActive() || !Building || !Building->HasAuthority())
	{
		return;
	}
	GrantBoost(Building);
	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, /*bWasCancelled=*/false);
}

void USupplyBoostAbility::GrantBoost(ABuildingBase* Building)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(Building->GetWorld()->GetAuthGameMode());
	const bool bHasCost = ConstructionCost.PrimaryCost || ConstructionCost.SecondaryCost || ConstructionCost.TertiaryCost
		|| ConstructionCost.RareCost || ConstructionCost.EpicCost || ConstructionCost.LegendaryCost;

	if (Building->AddSupplyCapacityBonus(CapacityBonus))
	{
		if (BoostSound)
		{
			Building->FireEffects(nullptr, BoostSound, FVector::OneVector, SoundVolume);
		}
		if (bDisableAfterUse)
		{
			SetAbilityEnabledByKeyForUnit(Building, AbilityKey, false);
		}
	}
	else if (bHasCost && GameMode)
	{
		// The building grants nothing (not finished, already dead, team at the cap): hand the price back.
		FBuildingCost Refund;
		Refund.PrimaryCost = -ConstructionCost.PrimaryCost;
		Refund.SecondaryCost = -ConstructionCost.SecondaryCost;
		Refund.TertiaryCost = -ConstructionCost.TertiaryCost;
		Refund.RareCost = -ConstructionCost.RareCost;
		Refund.EpicCost = -ConstructionCost.EpicCost;
		Refund.LegendaryCost = -ConstructionCost.LegendaryCost;
		GameMode->ModifyResourceCCost(Refund, Building->TeamId);
	}
}
