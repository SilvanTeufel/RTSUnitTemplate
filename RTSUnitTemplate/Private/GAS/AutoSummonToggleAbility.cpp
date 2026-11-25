// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/AutoSummonToggleAbility.h"

#include "Characters/Unit/UnitBase.h"

UAutoSummonToggleAbility::UAutoSummonToggleAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bStopMovementOnActivation = false;
	UseAbilityQue = false;
	AbilityName = TEXT("Auto Build");
}

bool UAutoSummonToggleAbility::ApplyToggle(AUnitBase* Unit) const
{
	if (!Unit || !Unit->HasAuthority())
	{
		return false;
	}

	Unit->bAutoSummonEnabled = bSwitchesOn;

	if (SwapTo)
	{
		const UClass* Mine = GetClass();
		for (TArray<TSubclassOf<UGameplayAbilityBase>>* Abilities : { &Unit->DefaultAbilities, &Unit->SecondAbilities, &Unit->ThirdAbilities, &Unit->FourthAbilities })
		{
			for (TSubclassOf<UGameplayAbilityBase>& Entry : *Abilities)
			{
				if (Entry.Get() == Mine)
				{
					Entry = SwapTo;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AutoSummon] %s: auto summon %s"), *Unit->GetName(), bSwitchesOn ? TEXT("on") : TEXT("off"));
	return true;
}

void UAutoSummonToggleAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Only reached when something activates it through GAS directly; the button path never gets here.
	if (ActorInfo)
	{
		ApplyToggle(Cast<AUnitBase>(ActorInfo->OwnerActor.Get()));
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
