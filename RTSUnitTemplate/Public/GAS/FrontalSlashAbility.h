// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "FrontalSlashAbility.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class AUnitBase;

/**
 * Melee strike in front of the caster (e.g. the EdgeDancer's "Blade Wave"): no target click. On activation the
 * cooldown is committed, SlashMontage plays on the caster for every machine (GAS montage replication, see
 * PlayMontageOnAvatar) and after HitDelay every living enemy inside a cone in front of the caster - SlashRange deep,
 * SlashHalfAngle to each side - takes the caster's AttackDamage times DamageMultiplier (minus its armor).
 * SlashVFX / SlashSound play in front of the caster at the moment of the hit (multicast).
 *
 * The cooldown is the usual CooldownGameplayEffectClass; CooldownResearchName shortens it for the whole team.
 * Nothing to wire in Blueprint: a Blueprint child sets the values, effects, montage and icon.
 */
UCLASS()
class RTSUNITTEMPLATE_API UFrontalSlashAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UFrontalSlashAbility();

	/** Damage = caster's AttackDamage x this, minus the target's armor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0"))
	float DamageMultiplier = 3.f;

	/** How far in front of the caster the slash reaches (from the caster's centre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0"))
	float SlashRange = 500.f;

	/** Half opening angle of the cone (degrees). 90 = everything in front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0", ClampMax = "180"))
	float SlashHalfAngle = 70.f;

	/**
	 * Cast before the slash, like the projectile abilities: the unit enters Casting (cast bar in CastTimerColor,
	 * CastChargeVFX / CastChargeSound for the duration) and the slash starts when the cast completes. Cooldown is
	 * committed only then, so a cancelled cast costs nothing. 0 = slash at once (old behaviour).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0"))
	float SlashCastTime = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	FLinearColor CastTimerColor = FLinearColor(0.f, 0.45f, 1.f, 1.f);

	/** Seconds from activation to the hit - the wind-up of the montage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0"))
	float HitDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	UAnimMontage* SlashMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0.05"))
	float MontagePlayRate = 1.f;

	/** Played at the hit, SlashVFXForwardOffset in front of the caster, turned like the caster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	UNiagaraSystem* SlashVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	FVector SlashVFXScale = FVector(1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	FRotator SlashVFXRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	float SlashVFXForwardOffset = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	float SlashVFXHeightOffset = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash", meta = (ClampMin = "0.1"))
	float SlashVFXLifetime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	USoundBase* SlashSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	float SlashSoundScale = 1.f;

	/** Played on every enemy that is hit. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	UNiagaraSystem* HitVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	FVector HitVFXScale = FVector(1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	USoundBase* HitSound = nullptr;

	/** Draws the cone (yellow) and the hit units (red). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Slash")
	bool bDrawDebug = false;

	/** Number of enemies the last slash hit (for tests and Blueprint feedback). */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Slash")
	int32 LastHitCount = 0;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * Aim mode (AbilityIndicatorClass set): a player's activation waits for the click and slashes towards it - the
	 * unit turns to the mouse meanwhile (bRotateUnitsToMouse). An activation without a player (AI) slashes at once,
	 * straight ahead or towards its target hit.
	 */
	virtual void OnAbilityMouseHit_Implementation(const FHitResult& InHitResult) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void HandleCastComplete(const FHitResult& InHitResult = FHitResult()) override;

private:
	void PerformSlash();
	/** Starts the cast (SlashCastTime > 0) or the slash right away. */
	void BeginCastOrSlash(AUnitBase* Caster);
	void StartSlash(AUnitBase* Caster);
	bool bSlashCasting = false;
	bool bWaitingForAim = false;
	/** Direction of the slash (flat); zero = the caster's facing. */
	FVector AimDirection = FVector::ZeroVector;
	void FinishAfterMontage();
	FTimerHandle SlashTimer;
	TWeakObjectPtr<AUnitBase> SlashCaster;
	float MontageDuration = 0.f;
};
