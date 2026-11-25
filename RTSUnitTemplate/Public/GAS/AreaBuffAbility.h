// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "AreaBuffAbility.generated.h"

class UMaterialInterface;

class UGameplayEffect;
class UNiagaraSystem;
class USoundBase;

/**
 * Instant area buff around the caster (e.g. "Overclock"): no target click, no cast. On activation the cooldown is
 * committed and FriendlyEffect is applied to every living friendly unit (same team or allied, the caster included)
 * whose Mass position lies within BuffRadius. The effect's own duration decides how long the buff lasts.
 *
 * The cooldown is the usual CooldownGameplayEffectClass; a research named in CooldownResearchName shortens it for
 * the whole team, for units already on the field and new ones alike (UGameplayAbilityBase::ApplyCooldown).
 *
 * Nothing to wire in Blueprint: a Blueprint child sets the effect, radius, cooldown, effects and the icon.
 */
UCLASS()
class RTSUNITTEMPLATE_API UAreaBuffAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	/** Applied to every friendly unit in BuffRadius (server only, replicates through GAS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	TSubclassOf<UGameplayEffect> FriendlyEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff", meta = (ClampMin = "0"))
	float BuffRadius = 1500.f;

	/**
	 * Above 1, the units in BuffRadius also attack this many times faster for FriendlyAttackSpeedDuration
	 * seconds (AUnitBase::ApplyAttackSpeedBuff - shorter PauseDuration/AttackDuration, animations stay whole).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff", meta = (ClampMin = "1.0"))
	float FriendlyAttackSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff", meta = (ClampMin = "0.0"))
	float FriendlyAttackSpeedDuration = 30.f;

	/** Played at the caster for everyone (multicast), removed after CastVFXLifetime. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	UNiagaraSystem* CastVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	FVector CastVFXScale = FVector(1.f);

	/** Seconds until CastVFX is removed again - also ends a looping system (an aura). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff", meta = (ClampMin = "0.1"))
	float CastVFXLifetime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	USoundBase* CastSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	float CastSoundScale = 1.f;

	/** Played on every buffed unit (multicast), removed after FriendlyVFXLifetime. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	UNiagaraSystem* FriendlyVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	FVector FriendlyVFXScale = FVector(1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff", meta = (ClampMin = "0.1"))
	float FriendlyVFXLifetime = 1.5f;

	/** Ring on the ground that marks BuffRadius (APerformanceUnit::MulticastShowRangeRing). None = no ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	UMaterialInterface* RangeRingMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	FLinearColor RangeRingColor = FLinearColor(1.f, 0.75f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff", meta = (ClampMin = "0.2"))
	float RangeRingLifetime = 2.f;

	/** Draws the affected sphere (yellow) and the buffed units (green). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AreaBuff")
	bool bDrawDebug = false;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
