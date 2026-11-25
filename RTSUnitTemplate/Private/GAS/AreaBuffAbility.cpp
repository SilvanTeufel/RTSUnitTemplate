// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/AreaBuffAbility.h"
#include "Characters/Unit/UnitBase.h"
#include "EngineUtils.h"
#include "System/PlayerTeamSubsystem.h"
#include "DrawDebugHelpers.h"

void UAreaBuffAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AUnitBase* Caster = ActorInfo ? Cast<AUnitBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Caster || !Caster->HasAuthority() || Caster->GetUnitState() == UnitData::Dead)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
		return;
	}

	// Cost + cooldown (the researched cooldown multiplier is applied in ApplyCooldown).
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	// Every living friendly unit (same team or allied) whose Mass position is inside the radius. Not a sphere
	// overlap: the actors of Mass units carry no collision a Pawn/WorldDynamic overlap finds (12.11.2026).
	if (FriendlyEffect || FriendlyAttackSpeedMultiplier > 1.f)
	{
		UWorld* World = Caster->GetWorld();
		int64 AlliedMask = 0;
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UPlayerTeamSubsystem* TeamSub = GI->GetSubsystem<UPlayerTeamSubsystem>())
			{
				AlliedMask = TeamSub->GetAlliedTeamsMask(Caster->TeamId);
			}
		}
		const FVector Center = Caster->GetMassActorLocation();
		const float RadiusSq = FMath::Square(BuffRadius);
		for (TActorIterator<AUnitBase> It(World); It; ++It)
		{
			AUnitBase* Unit = *It;
			if (!Unit || Unit->GetUnitState() == UnitData::Dead || !Unit->Attributes)
			{
				continue;
			}
			const int32 UnitTeam = Unit->TeamId;
			const bool bAllied = UnitTeam >= 0 && UnitTeam < 64 && (AlliedMask & (1LL << UnitTeam)) != 0;
			if (UnitTeam != Caster->TeamId && !bAllied)
			{
				continue;
			}
			const FVector UnitLocation = Unit->GetMassActorLocation();
			if (FVector::DistSquared(UnitLocation, Center) > RadiusSq)
			{
				continue;
			}
			if (FriendlyEffect)
			{
				Unit->ApplyInvestmentEffect(FriendlyEffect);
			}
			if (FriendlyAttackSpeedMultiplier > 1.f)
			{
				Unit->ApplyAttackSpeedBuff(FriendlyAttackSpeedMultiplier, FriendlyAttackSpeedDuration);
			}
			if (FriendlyVFX)
			{
				Caster->FireEffectsAtLocation(FriendlyVFX, nullptr, FriendlyVFXScale, 1.f, UnitLocation, FriendlyVFXLifetime);
			}
			if (bDrawDebug)
			{
				DrawDebugSphere(World, UnitLocation, 60.f, 8, FColor::Green, false, 3.f);
			}
		}
		if (bDrawDebug)
		{
			DrawDebugSphere(World, Center, BuffRadius, 24, FColor::Yellow, false, 3.f);
		}
	}

	if (CastVFX || CastSound)
	{
		Caster->FireEffectsAtLocation(CastVFX, CastSound, CastVFXScale, CastSoundScale, Caster->GetMassActorLocation(), CastVFXLifetime);
	}
	if (RangeRingMaterial)
	{
		Caster->MulticastShowRangeRing(RangeRingMaterial, Caster->GetMassActorLocation(), BuffRadius, RangeRingLifetime, RangeRingColor);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
}
