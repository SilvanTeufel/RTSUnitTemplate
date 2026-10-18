// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/ConversionSelectAbility.h"

#include "Characters/Unit/BuildingBase.h"

bool UConversionSelectAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ABuildingBase* Converter = ActorInfo ? Cast<ABuildingBase>(ActorInfo->OwnerActor.Get()) : nullptr;
	if (bSwitchOff)
	{
		if (!Converter || !Converter->IsConverter() || Converter->GetUnitState() == UnitData::Dead || !Converter->bHasConversion)
		{
			return false;
		}
		return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	}
	if (!Converter || !Converter->IsConverter() || Converter->GetUnitState() == UnitData::Dead
		|| InputType == EResourceType::MAX || OutputType == EResourceType::MAX || InputType == OutputType
		|| Converter->IsConversionActive(InputType, OutputType))
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UConversionSelectAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Deliberately not Super::ActivateAbility: that starts the cast / casting state of the ordinary
	// abilities. Picking a conversion is a switch, done at once.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (ABuildingBase* Converter = Cast<ABuildingBase>(ActorInfo->OwnerActor.Get()))
		{
			if (bSwitchOff)
			{
				Converter->ClearConversion();
			}
			else
			{
				Converter->SetConversion(InputType, OutputType);
			}
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
