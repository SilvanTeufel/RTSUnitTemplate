// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "LogisticsSummonAbility.generated.h"

class AUnitBase;

/**
 * Produces logistics units - the logistics counterpart of GA_Summon, cast included: one cast,
 * one logistics unit.
 *
 * Meant for the bases themselves (a StoreOnly depot, a CollectOnly outpost): with bAutoCast the
 * building casts it on its own whenever it stands idle, so neither the player nor the AI has to
 * remember to build haulers. It can still be triggered by hand like any other ability.
 *
 * Two limits, each off at 0:
 *   MaxSummonsPerOwner - living units THIS owner has summoned (its SummonedUnitsDataSet, as
 *                        GA_Summon counts them). The owner summons again when one dies.
 *   MaxLogisticsUnits  - every living logistics unit of the team plus summons being cast.
 *
 * Pays ConstructionCost when it starts; a cancelled cast is refunded (bRefundOnCancel).
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ULogisticsSummonAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	ULogisticsSummonAbility();

	/** The logistics unit to produce. Must have UnitRole = Logistics, or it would never count towards the limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	TSubclassOf<AUnitBase> LogisticsUnitClass;

	/** Living logistics units this owner keeps on the field; it summons again when one dies. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	int32 MaxSummonsPerOwner = 1;

	/** Team-wide cap on logistics units (built, plus being built). 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0"))
	int32 MaxLogisticsUnits = 0;

	/** Colour of the owner's cast timer while summoning (GA_Summon uses white). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	FLinearColor CastTimerColor = FLinearColor::White;

	/** The owner casts this on its own while it is idle and the team is below the limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	bool bAutoCast = true;

	/** How often the owner checks whether to cast again (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0.5", EditCondition = "bAutoCast"))
	float AutoCastInterval = 3.f;

	/** Cast duration in seconds - written to the owner's CastTime when the cast starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics", meta = (ClampMin = "0.1"))
	float SummonCastTime = 3.f;

	/** Living logistics units of a team plus those currently being summoned. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Logistics", meta = (WorldContext = "WorldContextObject"))
	static int32 CountLogisticsUnits(const UObject* WorldContextObject, int32 TeamId, bool bIncludePendingSummons = true);

	/** Living logistics units in the owner's SummonedUnitsDataSet - what MaxSummonsPerOwner counts. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Logistics")
	static int32 CountOwnerSummons(const AUnitBase* Owner);

	/** True while both limits leave room. bThisCastPending: the running cast of this ability counts too. */
	bool HasRoomForAnother(const AUnitBase* Owner, bool bThisCastPending) const;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void HandleCastComplete(const FHitResult& InHitResult = FHitResult()) override;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

private:
	void TryAutoCast();

	/** (Re)starts the auto-cast timer. From OnAvatarSet AND OnGiveAbility: an owner that re-grants its
	 *  abilities only calls the latter on the new instance, and the old one's timer died in OnRemoveAbility. */
	void StartAutoCastTimer(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec);

	FTimerHandle AutoCastTimer;
	FGameplayAbilitySpecHandle AutoCastSpecHandle;

	// Kept from the timer start: the ability's current actor info is not something to rely on
	// between activations.
	TWeakObjectPtr<AUnitBase> AutoCastOwner;
	TWeakObjectPtr<UAbilitySystemComponent> AutoCastASC;
};
