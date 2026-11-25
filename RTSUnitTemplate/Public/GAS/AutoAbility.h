// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "AutoAbility.generated.h"

class UNiagaraSystem;
class USoundBase;
class UMaterialInterface;
class AUnitBase;

/** When an automatic ability goes off by itself. */
UENUM(BlueprintType)
enum class EAutoAbilityTrigger : uint8
{
	/** Every attack of the unit (melee hit or shot) - TriggerChance per attack. */
	OnAttack	UMETA(DisplayName = "On Attack"),
	/** When the unit gets into a fight (an enemy in range, or its first attack). */
	OnEngage	UMETA(DisplayName = "On Engage"),
	/** When the unit takes a hit. */
	OnHit		UMETA(DisplayName = "When Hit"),
	/** When the unit takes a hit and its health is below LowHealthPercent. */
	LowHealth	UMETA(DisplayName = "Low Health"),
	/** Checked every second: goes off when there is something to do (an enemy in Radius, a hurt ally to heal). */
	Periodic	UMETA(DisplayName = "Periodic"),
};

/** Where the area of an automatic ability is centred. */
UENUM(BlueprintType)
enum class EAutoAbilityCenter : uint8
{
	Caster	UMETA(DisplayName = "Caster"),
	/** The unit attacked (OnAttack) or the attacker (OnHit); the caster when there is none. */
	Target	UMETA(DisplayName = "Target"),
};

/**
 * Automatic ability: goes off by itself on its Trigger, then cools down (Cooldown seconds, per unit). It is not a
 * button and never blocks the unit: it does not run through the unit's ability slot, it just hits, heals or buffs
 * the units around it - once, or as an aura that pulses for Duration seconds.
 *
 * Who has it: UAutoAbilitySubsystem. The team gets it for one unit type (Units.Tier.N) from an auto ability card
 * (UStartLoadoutComponent::AutoAbilityCardSets) or an auto ability node of the attribute tree, and a single unit
 * gets a random one at every AbilityPointsEveryXLevel levels. Chosen by card AND in the tree = half the cooldown.
 *
 * All values add up: a Blueprint child sets damage and/or heal and/or attack speed, the radius, the look and the
 * icon (AbilityIcon) and name (AbilityName). The card and tooltip text is written from the values
 * (GetAutoDescription) unless Description is set.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API UAutoAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UAutoAbility();

	// --- When ---------------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|When")
	EAutoAbilityTrigger Trigger = EAutoAbilityTrigger::OnEngage;

	/** Seconds until it can go off again (per unit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|When", meta = (ClampMin = "0"))
	float Cooldown = 20.f;

	/** Chance per trigger event (0-1), e.g. 0.25 = every fourth attack on average. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|When", meta = (ClampMin = "0", ClampMax = "1"))
	float TriggerChance = 1.f;

	/** LowHealth: goes off when hit below this share of max health (percent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|When", meta = (ClampMin = "1", ClampMax = "100"))
	float LowHealthPercent = 35.f;

	// --- Where --------------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Where")
	EAutoAbilityCenter Center = EAutoAbilityCenter::Caster;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Where", meta = (ClampMin = "0"))
	float Radius = 450.f;

	/** Seconds from going off to the first hit, e.g. a meteor that has to fall first. CastVFX plays at once; the
	 *  hit lands where the centre was when it went off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Where", meta = (ClampMin = "0"))
	float ImpactDelay = 0.f;

	/** Most enemies hit per pulse, the nearest first. 0 = all in Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Where", meta = (ClampMin = "0"))
	int32 MaxTargets = 0;

	// --- How long -----------------------------------------------------------------------------------------

	/** 0 = once. Above 0 = an aura: pulses every PulseInterval for this many seconds (it moves with the caster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Aura", meta = (ClampMin = "0"))
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Aura", meta = (ClampMin = "0.1"))
	float PulseInterval = 1.f;

	// --- What (adds up) -----------------------------------------------------------------------------------

	/** Damage per pulse to every enemy in Radius: the caster's AttackDamage times this ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect", meta = (ClampMin = "0"))
	float DamageMultiplier = 0.f;

	/** ... plus this (buildings without attack damage use it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect", meta = (ClampMin = "0"))
	float FlatDamage = 0.f;

	/** Off = the target's armor is taken off the damage (like a shot). Auras with small pulses ignore it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect")
	bool bIgnoreArmor = false;

	/** The caster heals itself by this percent of the damage it dealt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect", meta = (ClampMin = "0"))
	float LifestealPercent = 0.f;

	/** Heal per pulse, percent of max health, for every ally in Radius (the caster too). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect", meta = (ClampMin = "0"))
	float HealPercent = 0.f;

	/** Attack speed buff for the allies in Radius (1.3 = 30% faster), AttackSpeedSeconds long. 1 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect", meta = (ClampMin = "1"))
	float AttackSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect", meta = (ClampMin = "0"))
	float AttackSpeedSeconds = 6.f;

	/** Heal and attack speed only for the caster itself, not for the allies around it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Effect")
	bool bSelfOnly = false;

	// --- Look ---------------------------------------------------------------------------------------------

	/** Once, at the centre, when it goes off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	UNiagaraSystem* CastVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	FVector CastVFXScale = FVector(1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	USoundBase* CastSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	float CastSoundScale = 1.f;

	/** Every pulse of an aura (from the second one on), at the centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	UNiagaraSystem* PulseVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	FVector PulseVFXScale = FVector(1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	USoundBase* PulseSound = nullptr;

	/** On every enemy hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	UNiagaraSystem* HitVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	FVector HitVFXScale = FVector(1.f);

	/** On every ally healed or buffed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	UNiagaraSystem* AllyVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	FVector AllyVFXScale = FVector(1.f);

	/** Ring on the ground that marks Radius (APerformanceUnit::MulticastShowRangeRing); an aura shows it as long as
	 *  it lasts. None = no ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	UMaterialInterface* RangeRingMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	FLinearColor RangeRingColor = FLinearColor(1.f, 0.55f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look", meta = (ClampMin = "0.2"))
	float RangeRingLifetime = 1.5f;

	/** Height of the effects above the ground point of the units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	float VFXHeight = 0.f;

	/** Seconds the effects live. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look", meta = (ClampMin = "0.1"))
	float VFXLifetime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoAbility|Look")
	bool bDrawDebug = false;

	// --- Use ----------------------------------------------------------------------------------------------

	/** The name on cards and tooltips (AbilityName without its trailing colon and line breaks). */
	UFUNCTION(BlueprintPure, Category = "AutoAbility")
	FText GetAutoName() const;

	/** Card / tooltip text from the values (Description wins when set); CooldownMultiplier 0.5 = halved. */
	UFUNCTION(BlueprintPure, Category = "AutoAbility")
	FText GetAutoDescription(float CooldownMultiplier = 1.f) const;

	/**
	 * Server: makes it go off for Caster (on the class default object - every value is read-only here). Other is
	 * the attacked unit / the attacker. False = nothing to do (no enemy in reach, nobody hurt) - no cooldown then.
	 */
	bool Execute(AUnitBase* Caster, AUnitBase* Other) const;

	/** GAS path (granted like a normal ability and activated): goes off once at the caster, then ends. */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** One pulse. Returns how many units it did something to. */
	int32 Pulse(AUnitBase* Caster, const FVector& At, bool bFirst) const;
	bool HasWork(AUnitBase* Caster, const FVector& At) const;
	/** The first pulse at At, then the aura pulses (Duration). */
	void StartPulses(AUnitBase* Caster, AUnitBase* Other, const FVector& At) const;
	FVector CenterFor(AUnitBase* Caster, AUnitBase* Other) const;
};
