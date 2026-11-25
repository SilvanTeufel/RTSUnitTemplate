// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/FrontalSlashAbility.h"
#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "System/PlayerTeamSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Mass/UnitMassTag.h"

UFrontalSlashAbility::UFrontalSlashAbility()
{
	// Holds a timer until the hit: one instance per unit, never the CDO.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UFrontalSlashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AUnitBase* Caster = ActorInfo ? Cast<AUnitBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Caster || !Caster->HasAuthority() || Caster->GetUnitState() == UnitData::Dead)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
		return;
	}

	SlashCaster = Caster;
	AimDirection = FVector::ZeroVector;
	bWaitingForAim = false;
	bSlashCasting = false;
	// Aim mode: a player aims with the indicator first (OnAbilityMouseHit starts the slash).
	if (AbilityIndicatorClass && Caster->CurrentInstigatorPC.IsValid())
	{
		bWaitingForAim = true;
		return;
	}
	BeginCastOrSlash(Caster);
}

void UFrontalSlashAbility::OnAbilityMouseHit_Implementation(const FHitResult& InHitResult)
{
	AUnitBase* Caster = SlashCaster.Get();
	if (!bWaitingForAim || !Caster || !Caster->HasAuthority())
	{
		return;
	}
	bWaitingForAim = false;
	const FVector To = InHitResult.Location - Caster->GetMassActorLocation();
	const FVector Flat(To.X, To.Y, 0.f);
	if (Flat.SizeSquared() > 1.f)
	{
		AimDirection = Flat.GetSafeNormal();
	}
	BeginCastOrSlash(Caster);
}

void UFrontalSlashAbility::BeginCastOrSlash(AUnitBase* Caster)
{
	if (SlashCastTime <= KINDA_SMALL_NUMBER)
	{
		StartSlash(Caster);
		return;
	}
	// The cast, set up as the projectile abilities' StartCast does it: timer colour, a fresh control timer, the
	// cast time, the cast registered with the cast guard (plays CastChargeVFX / CastChargeSound) and the state.
	// The casting processor reports the end through HandleCastComplete.
	if (Caster->GetUnitState() == UnitData::Casting)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}
	Caster->SetTimerWidgetCastingColor(CastTimerColor);
	Caster->UnitControlTimer = 0.f;
	Caster->CastTime = SlashCastTime;
	bSlashCasting = true;
	AddCastingFallback();
	Caster->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
}

void UFrontalSlashAbility::HandleCastComplete(const FHitResult& InHitResult)
{
	Super::HandleCastComplete(InHitResult);
	AUnitBase* Caster = SlashCaster.Get();
	if (!bSlashCasting || !IsActive() || !Caster || !Caster->HasAuthority())
	{
		return;
	}
	bSlashCasting = false;
	// The cast is over - sign off from the cast guard, or AGASUnit::EnforceCastingInvariant pulls the unit back
	// into Casting for the rest of the swing (the ability lives on until the montage is through).
	RemoveCastingFallback();
	StartSlash(Caster);
}

void UFrontalSlashAbility::StartSlash(AUnitBase* Caster)
{
	const FGameplayAbilitySpecHandle Handle = CurrentSpecHandle;
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	const FGameplayAbilityActivationInfo ActivationInfo = CurrentActivationInfo;

	// Cost + cooldown (the researched cooldown multiplier is applied in ApplyCooldown).
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	// The swing: GAS replicates the montage to every client that sees the unit (RepAnimMontageInfo).
	MontageDuration = SlashMontage ? PlayMontageOnAvatar(SlashMontage, MontagePlayRate) : 0.f;

	if (HitDelay <= KINDA_SMALL_NUMBER)
	{
		PerformSlash();
		FinishAfterMontage();
		return;
	}
	Caster->GetWorldTimerManager().SetTimer(SlashTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		PerformSlash();
		FinishAfterMontage();
	}), HitDelay, false);
}

void UFrontalSlashAbility::FinishAfterMontage()
{
	// The ability owns the montage (GAS montage replication): ending it right at the hit stopped the swing
	// halfway on every machine. Keep it alive until the montage is through.
	const float Rest = MontageDuration - HitDelay;
	AUnitBase* Caster = SlashCaster.Get();
	if (Rest > 0.05f && Caster)
	{
		Caster->GetWorldTimerManager().SetTimer(SlashTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/false);
		}), Rest, false);
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled=*/false);
}

void UFrontalSlashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AUnitBase* Caster = SlashCaster.Get())
	{
		Caster->GetWorldTimerManager().ClearTimer(SlashTimer);
	}
	SlashCaster.Reset();
	bSlashCasting = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFrontalSlashAbility::PerformSlash()
{
	AUnitBase* Caster = SlashCaster.Get();
	LastHitCount = 0;
	if (!Caster || !Caster->HasAuthority() || Caster->GetUnitState() == UnitData::Dead || !Caster->Attributes)
	{
		return;
	}
	UWorld* World = Caster->GetWorld();

	const FVector Origin = Caster->GetMassActorLocation();
	const FRotator Facing(0.f, AimDirection.IsNearlyZero() ? Caster->GetMassActorRotation().Yaw : AimDirection.Rotation().Yaw, 0.f);
	const FVector Forward = Facing.Vector();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SlashHalfAngle, 0.f, 180.f)));
	const float Damage = Caster->Attributes->GetAttackDamage() * DamageMultiplier;

	int64 AlliedMask = 0;
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UPlayerTeamSubsystem* TeamSub = GI->GetSubsystem<UPlayerTeamSubsystem>())
		{
			AlliedMask = TeamSub->GetAlliedTeamsMask(Caster->TeamId);
		}
	}

	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Target = *It;
		if (!Target || Target == Caster || !Target->Attributes || Target->GetUnitState() == UnitData::Dead)
		{
			continue;
		}
		const int32 TargetTeam = Target->TeamId;
		const bool bAllied = TargetTeam >= 0 && TargetTeam < 64 && (AlliedMask & (1LL << TargetTeam)) != 0;
		if (TargetTeam == Caster->TeamId || bAllied)
		{
			continue;
		}
		const FVector ToTarget = Target->GetMassActorLocation() - Origin;
		const FVector Flat(ToTarget.X, ToTarget.Y, 0.f);
		const float Dist = Flat.Size();
		if (Dist > SlashRange)
		{
			continue;
		}
		// Units right on top of the caster count as in front.
		if (Dist > 1.f && FVector::DotProduct(Flat / Dist, Forward) < CosHalf)
		{
			continue;
		}

		// Same rules as a projectile hit (AUnitBase::ProjectileHit): armor first, then shield, then health.
		Target->Attacked(Caster);
		const float NewDamage = FMath::Max(0.f, Damage - Target->Attributes->GetArmor());
		if (Target->Attributes->GetShield() <= 0.f)
		{
			Target->SetHealth_Implementation(Target->Attributes->GetHealth() - NewDamage);
		}
		else
		{
			Target->SetShield_Implementation(Target->Attributes->GetShield() - NewDamage);
		}
		Caster->IncreaseExperience();
		++LastHitCount;

		if (HitVFX || HitSound)
		{
			Caster->FireEffectsAtLocation(HitVFX, HitSound, HitVFXScale, 1.f, Target->GetMassActorLocation(), 1.5f);
		}
		if (bDrawDebug)
		{
			DrawDebugSphere(World, Target->GetMassActorLocation(), 60.f, 8, FColor::Red, false, 3.f);
		}
	}

	if (SlashVFX || SlashSound)
	{
		const FVector At = Origin + Forward * SlashVFXForwardOffset + FVector(0.f, 0.f, SlashVFXHeightOffset);
		Caster->FireEffectsAtLocation(SlashVFX, SlashSound, SlashVFXScale, SlashSoundScale, At, SlashVFXLifetime, Facing + SlashVFXRotationOffset);
	}
	if (bDrawDebug)
	{
		DrawDebugCone(World, Origin, Forward, SlashRange, FMath::DegreesToRadians(SlashHalfAngle), FMath::DegreesToRadians(10.f), 16, FColor::Yellow, false, 3.f);
	}
}
