// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/AutoAbility.h"
#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "System/PlayerTeamSubsystem.h"
#include "DrawDebugHelpers.h"

namespace
{
	bool IsAlive(const AUnitBase* Unit)
	{
		return IsValid(Unit) && Unit->Attributes && Unit->GetUnitState() != UnitData::Dead && Unit->Attributes->GetHealth() > 0.f;
	}

	int64 AlliedMaskOf(const AUnitBase* Caster)
	{
		if (UGameInstance* GI = Caster->GetWorld() ? Caster->GetWorld()->GetGameInstance() : nullptr)
		{
			if (UPlayerTeamSubsystem* TeamSub = GI->GetSubsystem<UPlayerTeamSubsystem>())
			{
				return TeamSub->GetAlliedTeamsMask(Caster->TeamId);
			}
		}
		return 0;
	}

	bool IsFriend(const AUnitBase* Caster, const AUnitBase* Unit, int64 AlliedMask)
	{
		const int32 Team = Unit->TeamId;
		return Team == Caster->TeamId || (Team >= 0 && Team < 64 && (AlliedMask & (1LL << Team)) != 0);
	}

	float FlatDistance(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
	}

	FString Percent(float Value)
	{
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value));
	}

	FString Seconds(float Value)
	{
		return FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value)) ? FString::Printf(TEXT("%d s"), FMath::RoundToInt(Value)) : FString::Printf(TEXT("%.1f s"), Value);
	}
}

UAutoAbility::UAutoAbility()
{
	// Nothing per activation is stored on an instance: Execute runs on the class default object.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	AbilityName = TEXT("Auto Ability");
}

FText UAutoAbility::GetAutoName() const
{
	FString Name = AbilityName;
	Name.ReplaceInline(TEXT("\n"), TEXT(" "));
	Name.TrimStartAndEndInline();
	Name.RemoveFromEnd(TEXT(":"));
	Name.TrimStartAndEndInline();
	return FText::FromString(Name.IsEmpty() ? GetClass()->GetName() : Name);
}

FText UAutoAbility::GetAutoDescription(float CooldownMultiplier) const
{
	FString Text;
	if (!Description.IsEmpty())
	{
		Text = Description.ToString();
	}
	else
	{
		switch (Trigger)
		{
		case EAutoAbilityTrigger::OnAttack:
			Text = TriggerChance < 1.f ? FString::Printf(TEXT("On attack (%s chance): "), *Percent(TriggerChance * 100.f)) : TEXT("On attack: ");
			break;
		case EAutoAbilityTrigger::OnEngage: Text = TEXT("When entering combat: "); break;
		case EAutoAbilityTrigger::OnHit:
			Text = TriggerChance < 1.f ? FString::Printf(TEXT("When hit (%s chance): "), *Percent(TriggerChance * 100.f)) : TEXT("When hit: ");
			break;
		case EAutoAbilityTrigger::LowHealth: Text = FString::Printf(TEXT("When hit below %s health: "), *Percent(LowHealthPercent)); break;
		default: Text = TEXT("Automatically, when needed: "); break;
		}

		TArray<FString> Parts;
		const bool bAura = Duration > 0.f;
		const FString Where = Center == EAutoAbilityCenter::Target ? FString::Printf(TEXT("within %d around the target"), FMath::RoundToInt(Radius))
			: FString::Printf(TEXT("within %d"), FMath::RoundToInt(Radius));
		if (DamageMultiplier > 0.f || FlatDamage > 0.f)
		{
			FString Amount;
			if (DamageMultiplier > 0.f)
			{
				Amount = FString::Printf(TEXT("%s attack damage"), *Percent(DamageMultiplier * 100.f));
			}
			if (FlatDamage > 0.f)
			{
				Amount += FString::Printf(TEXT("%s%d damage"), Amount.IsEmpty() ? TEXT("") : TEXT(" + "), FMath::RoundToInt(FlatDamage));
			}
			FString Targets = MaxTargets == 1 ? TEXT("the nearest enemy") : (MaxTargets > 1 ? FString::Printf(TEXT("up to %d enemies"), MaxTargets) : TEXT("all enemies"));
			Parts.Add(FString::Printf(TEXT("deals %s to %s %s"), *Amount, *Targets, *Where));
			if (LifestealPercent > 0.f)
			{
				Parts.Add(FString::Printf(TEXT("heals itself for %s of it"), *Percent(LifestealPercent)));
			}
		}
		if (HealPercent > 0.f)
		{
			Parts.Add(bSelfOnly ? FString::Printf(TEXT("heals itself by %s of its max health"), *Percent(HealPercent))
				: FString::Printf(TEXT("heals allies %s by %s of their max health"), *Where, *Percent(HealPercent)));
		}
		if (AttackSpeedMultiplier > 1.f)
		{
			const FString Bonus = Percent((AttackSpeedMultiplier - 1.f) * 100.f);
			Parts.Add(bSelfOnly ? FString::Printf(TEXT("+%s attack speed for itself for %s"), *Bonus, *Seconds(AttackSpeedSeconds))
				: FString::Printf(TEXT("+%s attack speed for allies %s for %s"), *Bonus, *Where, *Seconds(AttackSpeedSeconds)));
		}
		Text += FString::Join(Parts, TEXT(", "));
		if (bAura)
		{
			Text += FString::Printf(TEXT(" - every %s for %s"), *Seconds(PulseInterval), *Seconds(Duration));
		}
		if (ImpactDelay > 0.f)
		{
			Text += FString::Printf(TEXT(" (hits after %s)"), *Seconds(ImpactDelay));
		}
		Text += TEXT(".");
	}

	const float Effective = Cooldown * FMath::Clamp(CooldownMultiplier, 0.05f, 1.f);
	Text += FString::Printf(TEXT("\nCooldown: %s"), *Seconds(Effective));
	if (CooldownMultiplier < 0.999f)
	{
		Text += TEXT(" (halved: chosen as card and in the attribute tree)");
	}
	return FText::FromString(Text);
}

FVector UAutoAbility::CenterFor(AUnitBase* Caster, AUnitBase* Other) const
{
	if (Center == EAutoAbilityCenter::Target && IsAlive(Other))
	{
		return Other->GetMassActorLocation();
	}
	return Caster->GetMassActorLocation();
}

bool UAutoAbility::HasWork(AUnitBase* Caster, const FVector& At) const
{
	const bool bDamage = DamageMultiplier > 0.f || FlatDamage > 0.f;
	const bool bHeal = HealPercent > 0.f;
	const bool bBuff = AttackSpeedMultiplier > 1.f;
	const int64 AlliedMask = AlliedMaskOf(Caster);
	// A buff is worth it when there is a fight: an enemy within reach (or the trigger already says so).
	const bool bNeedsEnemy = bDamage || (bBuff && Trigger == EAutoAbilityTrigger::Periodic);
	if (bHeal && bSelfOnly && Caster->Attributes->GetHealth() < Caster->Attributes->GetMaxHealth())
	{
		return true;
	}
	if (!bNeedsEnemy && !bHeal)
	{
		return true;
	}
	const float EnemyReach = Radius + (Center == EAutoAbilityCenter::Caster ? 0.f : 0.f);
	for (TActorIterator<AUnitBase> It(Caster->GetWorld()); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsAlive(Unit))
		{
			continue;
		}
		const bool bFriend = IsFriend(Caster, Unit, AlliedMask);
		const float Dist = FlatDistance(Unit->GetMassActorLocation(), At);
		if (Dist > EnemyReach)
		{
			continue;
		}
		if (!bFriend && bNeedsEnemy)
		{
			return true;
		}
		if (bFriend && bHeal && !bSelfOnly && Unit->Attributes->GetHealth() < Unit->Attributes->GetMaxHealth() * 0.98f)
		{
			return true;
		}
	}
	return !bNeedsEnemy && !bHeal;
}

bool UAutoAbility::Execute(AUnitBase* Caster, AUnitBase* Other) const
{
	if (!IsAlive(Caster) || !Caster->HasAuthority())
	{
		return false;
	}
	UWorld* World = Caster->GetWorld();
	const FVector At = CenterFor(Caster, Other);
	if (!HasWork(Caster, At))
	{
		return false;
	}

	if (CastVFX || CastSound)
	{
		Caster->FireEffectsAtLocation(CastVFX, CastSound, CastVFXScale, CastSoundScale, At + FVector(0.f, 0.f, VFXHeight), VFXLifetime);
	}
	if (RangeRingMaterial && Radius > 0.f)
	{
		Caster->MulticastShowRangeRing(RangeRingMaterial, At, Radius, FMath::Max(RangeRingLifetime, Duration + ImpactDelay), RangeRingColor);
	}
	if (ImpactDelay > 0.f && World)
	{
		// The hit comes later (a falling meteor) - at the point it was aimed at.
		TWeakObjectPtr<AUnitBase> WeakCaster(Caster);
		TWeakObjectPtr<AUnitBase> WeakOther(Other);
		TWeakObjectPtr<const UAutoAbility> WeakSelf(this);
		FTimerHandle DelayHandle;
		World->GetTimerManager().SetTimer(DelayHandle, FTimerDelegate::CreateLambda([WeakCaster, WeakOther, WeakSelf, At]()
		{
			const UAutoAbility* Self = WeakSelf.Get();
			AUnitBase* C = WeakCaster.Get();
			if (Self && IsAlive(C))
			{
				Self->StartPulses(C, WeakOther.Get(), At);
			}
		}), ImpactDelay, false);
		return true;
	}
	StartPulses(Caster, Other, At);
	return true;
}

void UAutoAbility::StartPulses(AUnitBase* Caster, AUnitBase* Other, const FVector& At) const
{
	UWorld* World = Caster->GetWorld();
	Pulse(Caster, At, true);

	const int32 Pulses = Duration > 0.f ? FMath::Max(1, FMath::RoundToInt(Duration / FMath::Max(0.1f, PulseInterval))) : 1;
	if (Pulses > 1 && World)
	{
		TWeakObjectPtr<AUnitBase> WeakCaster(Caster);
		TWeakObjectPtr<AUnitBase> WeakOther(Other);
		TWeakObjectPtr<const UAutoAbility> WeakSelf(this);
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TSharedRef<int32> Left = MakeShared<int32>(Pulses - 1);
		TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
		World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakCaster, WeakOther, WeakSelf, WeakWorld, Left, Handle]()
		{
			AUnitBase* C = WeakCaster.Get();
			const UAutoAbility* Self = WeakSelf.Get();
			if (!Self || !IsAlive(C) || --(*Left) < 0)
			{
				if (UWorld* W = WeakWorld.Get())
				{
					W->GetTimerManager().ClearTimer(*Handle);
				}
				return;
			}
			Self->Pulse(C, Self->CenterFor(C, WeakOther.Get()), false);
		}), FMath::Max(0.1f, PulseInterval), true);
	}
}

int32 UAutoAbility::Pulse(AUnitBase* Caster, const FVector& At, bool bFirst) const
{
	UWorld* World = Caster->GetWorld();
	if (!World)
	{
		return 0;
	}
	if (!bFirst && (PulseVFX || PulseSound))
	{
		Caster->FireEffectsAtLocation(PulseVFX, PulseSound, PulseVFXScale, CastSoundScale, At + FVector(0.f, 0.f, VFXHeight), VFXLifetime);
	}

	const int64 AlliedMask = AlliedMaskOf(Caster);
	const float Damage = (Caster->Attributes ? Caster->Attributes->GetAttackDamage() : 0.f) * DamageMultiplier + FlatDamage;
	const bool bHeal = HealPercent > 0.f;
	const bool bBuff = AttackSpeedMultiplier > 1.f;

	TArray<TPair<float, AUnitBase*>> Enemies;
	TArray<AUnitBase*> Friends;
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (!IsAlive(Unit))
		{
			continue;
		}
		const float Dist = FlatDistance(Unit->GetMassActorLocation(), At);
		if (Dist > Radius && Unit != Caster)
		{
			continue;
		}
		if (IsFriend(Caster, Unit, AlliedMask))
		{
			if (!bSelfOnly || Unit == Caster)
			{
				Friends.Add(Unit);
			}
		}
		else if (Damage > 0.f)
		{
			Enemies.Emplace(Dist, Unit);
		}
	}

	int32 Affected = 0;
	float Dealt = 0.f;
	if (Damage > 0.f && Enemies.Num() > 0)
	{
		Enemies.Sort([](const TPair<float, AUnitBase*>& A, const TPair<float, AUnitBase*>& B) { return A.Key < B.Key; });
		const int32 Count = MaxTargets > 0 ? FMath::Min(MaxTargets, Enemies.Num()) : Enemies.Num();
		for (int32 i = 0; i < Count; ++i)
		{
			AUnitBase* Target = Enemies[i].Value;
			// Like a shot (AUnitBase::ProjectileHit): armor first, then the shield, then health.
			Target->Attacked(Caster);
			const float NewDamage = FMath::Max(0.f, bIgnoreArmor ? Damage : Damage - Target->Attributes->GetArmor());
			if (Target->Attributes->GetShield() <= 0.f)
			{
				Target->SetHealth_Implementation(Target->Attributes->GetHealth() - NewDamage);
			}
			else
			{
				Target->SetShield_Implementation(Target->Attributes->GetShield() - NewDamage);
			}
			Dealt += NewDamage;
			++Affected;
			if (HitVFX)
			{
				Caster->FireEffectsAtLocation(HitVFX, nullptr, HitVFXScale, 1.f, Target->GetMassActorLocation() + FVector(0.f, 0.f, VFXHeight), VFXLifetime);
			}
		}
		Caster->IncreaseExperience();
	}

	if (bHeal || bBuff)
	{
		for (AUnitBase* Friend : Friends)
		{
			bool bDid = false;
			if (bHeal && Friend->Attributes->GetHealth() < Friend->Attributes->GetMaxHealth())
			{
				const float Amount = Friend->Attributes->GetMaxHealth() * HealPercent / 100.f;
				Friend->SetHealth_Implementation(FMath::Min(Friend->Attributes->GetMaxHealth(), Friend->Attributes->GetHealth() + Amount));
				bDid = true;
			}
			if (bBuff && !Friend->bIsBuilding)
			{
				Friend->ApplyAttackSpeedBuff(AttackSpeedMultiplier, AttackSpeedSeconds);
				bDid = true;
			}
			if (bDid)
			{
				++Affected;
				if (AllyVFX && (bFirst || bHeal))
				{
					Caster->FireEffectsAtLocation(AllyVFX, nullptr, AllyVFXScale, 1.f, Friend->GetMassActorLocation() + FVector(0.f, 0.f, VFXHeight), VFXLifetime);
				}
			}
		}
	}

	if (LifestealPercent > 0.f && Dealt > 0.f && IsAlive(Caster))
	{
		const float Max = Caster->Attributes->GetMaxHealth();
		Caster->SetHealth_Implementation(FMath::Min(Max, Caster->Attributes->GetHealth() + Dealt * LifestealPercent / 100.f));
	}

	if (bDrawDebug)
	{
		DrawDebugCircle(World, At + FVector(0.f, 0.f, 20.f), Radius, 48, FColor::Orange, false, FMath::Max(1.f, PulseInterval), 0, 6.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[AutoAbility] %s by %s (team %d): %d affected, %.0f damage."), *GetName(), *Caster->GetName(), Caster->TeamId, Affected, Dealt);
	return Affected;
}

void UAutoAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	AUnitBase* Caster = ActorInfo ? Cast<AUnitBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Caster && Caster->HasAuthority() && CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		GetClass()->GetDefaultObject<UAutoAbility>()->Execute(Caster, nullptr);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
}
