// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "AutoSummonToggleAbility.generated.h"

class AUnitBase;

/**
 * Button that switches the owner's automatic summoning (ULogisticsSummonAbility with bAutoCast:
 * workers, logistics units) on or off - AUnitBase::bAutoSummonEnabled.
 *
 * Make two Blueprint children, one per state, each with the icon of the state it SHOWS:
 *   "Auto build: on"  - bSwitchesOn = false, SwapTo = the "off" child (put this one in the building's ability array)
 *   "Auto build: off" - bSwitchesOn = true,  SwapTo = the "on" child
 * Pressing it switches and puts SwapTo into its slot, so the button always shows the current state.
 *
 * Instant and outside the GAS activation (AGASUnit::ActivateAbilityByInputID calls ApplyToggle): it
 * must work while the building is casting its summon, and it never blocks or queues anything.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API UAutoSummonToggleAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UAutoSummonToggleAbility();

	/** What pressing it does: true = switches auto summon on, false = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AutoSummon")
	bool bSwitchesOn = false;

	/** Takes this button's place in the owner's ability arrays after the switch (the counterpart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AutoSummon")
	TSubclassOf<UGameplayAbilityBase> SwapTo;

	/** Server: switches the unit and swaps the button. False when there is no unit or no authority. */
	bool ApplyToggle(AUnitBase* Unit) const;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
