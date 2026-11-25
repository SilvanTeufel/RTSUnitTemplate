// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "Core/UnitData.h"
#include "SupplyBoostAbility.generated.h"

/**
 * One-off upgrade of a supply building (e.g. the reactor's "Overcharge"): pays ConstructionCost and raises the
 * capacity the building grants by CapacityBonus (ABuildingBase::AddSupplyCapacityBonus). The bonus belongs to
 * the building - it is handed back with the rest of its capacity when the building dies. With bDisableAfterUse
 * the ability switches itself off for this building once it succeeded.
 *
 * With BoostCastTime > 0 the building casts first (its cast bar, UnitTimerWidget): the price is paid at the start,
 * the bonus comes when the cast completes (OnAbilityCastComplete fires for the Blueprint as well), a cancelled cast
 * is refunded.
 *
 * Nothing to wire in Blueprint: a Blueprint child only sets the cost, the bonus, the key and the icon.
 */
UCLASS()
class RTSUNITTEMPLATE_API USupplyBoostAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	/** Added to the building's granted capacity (the energy of the Singularians is Rare). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	FBuildingCost CapacityBonus;

	/** Once per building: afterwards the ability is disabled for it (per owner, by AbilityKey). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	bool bDisableAfterUse = true;

	/** Seconds the building casts before the bonus is granted. 0 = at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply", meta = (ClampMin = "0"))
	float BoostCastTime = 4.f;

	/** Colour of the building's cast bar while it casts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	FLinearColor CastTimerColor = FLinearColor(0.75f, 0.3f, 1.f, 1.f);

	/** Played on the building when the cast starts (about BoostCastTime long). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	USoundBase* CastSound = nullptr;

	/** Played once the boost is granted (short). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	USoundBase* BoostSound = nullptr;

	/** Volume of CastSound and BoostSound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply", meta = (ClampMin = "0"))
	float SoundVolume = 0.6f;

	USupplyBoostAbility();

	virtual void HandleCastComplete(const FHitResult& InHitResult = FHitResult()) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** Adds the bonus (or hands the price back when the building grants nothing). Server only. */
	void GrantBoost(class ABuildingBase* Building);
};
