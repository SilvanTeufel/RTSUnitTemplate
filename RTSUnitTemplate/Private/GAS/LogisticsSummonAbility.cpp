// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/LogisticsSummonAbility.h"

#include "AbilitySystemComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "EngineUtils.h"
#include "GameModes/ResourceGameMode.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
	bool LogisticsLogEnabled()
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("RTS.Logistics.Log"));
		return CVar && CVar->GetInt() > 0;
	}
}

ULogisticsSummonAbility::ULogisticsSummonAbility()
{
	// One instance per owner: it carries the auto-cast timer.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// The base class enters and leaves the Casting state for us and the Mass casting processor
	// reports the end through HandleCastComplete - no Blueprint graph needed.
	bUseCastingFallbackProcessor = true;
	bRefundOnCancel = true;
	bStopMovementOnActivation = true;
	AbilityName = TEXT("Logistics Unit");
}

int32 ULogisticsSummonAbility::CountLogisticsUnits(const UObject* WorldContextObject, int32 TeamId, bool bIncludePendingSummons)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		const AUnitBase* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId != TeamId || Unit->GetUnitState() == UnitData::Dead)
		{
			continue;
		}
		if (Unit->IsLogisticsUnit())
		{
			++Count;
		}
		// A summon in progress is a hauler that already exists as far as the limit is concerned.
		if (bIncludePendingSummons)
		{
			const ULogisticsSummonAbility* Summon = Cast<ULogisticsSummonAbility>(Unit->ActivatedAbilityInstance);
			if (Summon && Summon->IsActive())
			{
				++Count;
			}
		}
	}
	return Count;
}

int32 ULogisticsSummonAbility::CountOwnerSummons(const AUnitBase* Owner)
{
	if (!Owner)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FUnitSpawnData& Data : Owner->SummonedUnitsDataSet)
	{
		if (IsValid(Data.UnitBase) && Data.UnitBase->GetUnitState() != UnitData::Dead && Data.UnitBase->IsLogisticsUnit())
		{
			++Count;
		}
	}
	return Count;
}

bool ULogisticsSummonAbility::HasRoomForAnother(const AUnitBase* Owner, bool bThisCastPending) const
{
	if (!Owner)
	{
		return false;
	}
	// The cast in question - about to start, or running and not spawned yet - is never in the data
	// set, so it always adds one.
	if (MaxSummonsPerOwner > 0 && CountOwnerSummons(Owner) + 1 > MaxSummonsPerOwner)
	{
		return false;
	}
	// The team count already includes running summon casts - this one too while it is pending.
	if (MaxLogisticsUnits > 0 && CountLogisticsUnits(Owner, Owner->TeamId) + (bThisCastPending ? 0 : 1) > MaxLogisticsUnits)
	{
		return false;
	}
	return true;
}

bool ULogisticsSummonAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!LogisticsUnitClass || !ActorInfo)
	{
		return false;
	}

	const AUnitBase* Owner = Cast<AUnitBase>(ActorInfo->OwnerActor.Get());
	if (!Owner || Owner->GetUnitState() == UnitData::Dead || !HasRoomForAnother(Owner, /*bThisCastPending=*/false))
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void ULogisticsSummonAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AUnitBase* Owner = ActorInfo ? Cast<AUnitBase>(ActorInfo->OwnerActor.Get()) : nullptr;

	// Pay first, like the Blueprint production abilities do. Not affordable: end without a refund.
	AResourceGameMode* GameMode = Owner ? Cast<AResourceGameMode>(Owner->GetWorld()->GetAuthGameMode()) : nullptr;
	const bool bHasCost = ConstructionCost.PrimaryCost || ConstructionCost.SecondaryCost || ConstructionCost.TertiaryCost
		|| ConstructionCost.RareCost || ConstructionCost.EpicCost || ConstructionCost.LegendaryCost;
	if (!Owner || (bHasCost && (!GameMode || !GameMode->ModifyResourceCCost(ConstructionCost, Owner->TeamId))))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
		return;
	}

	// The cast, set up as GA_Summon does it: timer colour, a fresh control timer and the cast time
	// (read by the casting processor through the combat-stats sync). The Casting state itself is
	// entered by the base class (bUseCastingFallbackProcessor), and the casting processor reports
	// the end through HandleCastComplete.
	Owner->SetTimerWidgetCastingColor(CastTimerColor);
	Owner->UnitControlTimer = 0.f;
	Owner->CastTime = SummonCastTime;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void ULogisticsSummonAbility::HandleCastComplete(const FHitResult& InHitResult)
{
	Super::HandleCastComplete(InHitResult);

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AUnitBase* Owner = ActorInfo ? Cast<AUnitBase>(ActorInfo->OwnerActor.Get()) : nullptr;
	if (!IsActive() || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	// The limits may have filled up during the cast (another owner was faster) - cancel, which refunds.
	if (!HasRoomForAnother(Owner, /*bThisCastPending=*/true))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, /*bWasCancelled=*/true);
		return;
	}

	FRotator MeshRotation = FRotator::ZeroRotator;
	if (const AUnitBase* UnitCDO = LogisticsUnitClass->GetDefaultObject<AUnitBase>())
	{
		if (const USkeletalMeshComponent* Mesh = UnitCDO->GetMesh())
		{
			MeshRotation = Mesh->GetRelativeRotation();
		}
	}

	Owner->SpawnUnitsFromParameters(LogisticsUnitClass, nullptr, nullptr, MeshRotation, Owner->GetMassActorLocation(),
		UnitData::Idle, UnitData::Idle, Owner->TeamId, ConstructionCost, /*Waypoint=*/nullptr, /*UnitCount=*/1,
		/*SummonContinuously=*/false, /*SpawnAsSquad=*/false, /*UseSummonDataSet=*/true, /*bSelectable=*/true);

	if (LogisticsLogEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("[LogisticsSummon] %s summoned a %s (own %d/%d, team %d)"), *Owner->GetName(),
			*GetNameSafe(LogisticsUnitClass.Get()), CountOwnerSummons(Owner), MaxSummonsPerOwner, CountLogisticsUnits(Owner, Owner->TeamId, false));
	}

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, /*bWasCancelled=*/false);
}

void ULogisticsSummonAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	StartAutoCastTimer(ActorInfo, Spec);
}

void ULogisticsSummonAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	StartAutoCastTimer(ActorInfo, Spec);
}

void ULogisticsSummonAbility::StartAutoCastTimer(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (!bAutoCast || !ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	AutoCastSpecHandle = Spec.Handle;
	AutoCastOwner = Cast<AUnitBase>(Avatar);
	AutoCastASC = ActorInfo->AbilitySystemComponent.Get();
	// Bound to the OWNER, not to this ability: UGameplayAbility::EndAbility calls
	// ClearAllTimersForObject(this), which killed a timer bound here after the very first cast.
	TWeakObjectPtr<ULogisticsSummonAbility> WeakThis(this);
	World->GetTimerManager().SetTimer(AutoCastTimer, FTimerDelegate::CreateWeakLambda(Avatar, [WeakThis]()
	{
		if (ULogisticsSummonAbility* Ability = WeakThis.Get())
		{
			Ability->TryAutoCast();
		}
	}), AutoCastInterval, true, AutoCastInterval);
}

void ULogisticsSummonAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (UWorld* World = ActorInfo->AvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoCastTimer);
		}
	}
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void ULogisticsSummonAbility::TryAutoCast()
{
	AUnitBase* Owner = AutoCastOwner.Get();
	UAbilitySystemComponent* ASC = AutoCastASC.Get();
	const bool bLog = LogisticsLogEnabled();
	if (!Owner || !ASC || !Owner->HasAuthority())
	{
		if (bLog)
		{
			UE_LOG(LogTemp, Log, TEXT("[LogisticsSummon] %s: auto-cast has no owner/ASC any more"), *GetName());
		}
		return;
	}

	auto Skip = [&](const TCHAR* Reason)
	{
		if (bLog)
		{
			UE_LOG(LogTemp, Log, TEXT("[LogisticsSummon] %s skips auto-cast: %s"), *Owner->GetName(), Reason);
		}
	};

	if (IsActive())
	{
		Skip(TEXT("already casting"));
		return;
	}

	// Only when the owner has nothing else to do: never interrupt a production, a cast, a move or a
	// fight. A building producing something else simply gets its turn after that.
	if (Owner->GetUnitState() != UnitData::Idle || Owner->ActivatedAbilityInstance != nullptr || Owner->IsInsideTransport)
	{
		Skip(Owner->ActivatedAbilityInstance ? TEXT("another ability is active") : TEXT("owner is busy"));
		return;
	}

	if (Owner->GetUnitState() == UnitData::Dead || !HasRoomForAnother(Owner, /*bThisCastPending=*/false))
	{
		return;   // the normal case once the fleet is complete - not worth a log line
	}

	if (AResourceGameMode* GameMode = Cast<AResourceGameMode>(Owner->GetWorld()->GetAuthGameMode()))
	{
		if (!GameMode->CanAffordConstruction(ConstructionCost, Owner->TeamId))
		{
			Skip(TEXT("cannot afford"));
			return;
		}
	}

	const bool bStarted = ASC->TryActivateAbility(AutoCastSpecHandle);
	if (bLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[LogisticsSummon] %s auto-cast %s (own %d/%d)"), *Owner->GetName(),
			bStarted ? TEXT("started") : TEXT("REFUSED by CanActivateAbility"), CountOwnerSummons(Owner), MaxSummonsPerOwner);
	}
}
