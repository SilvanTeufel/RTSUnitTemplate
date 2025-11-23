// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/AbilityButton.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Widgets/UnitWidgetSelector.h"
#include "GameFramework/PlayerController.h"
#include "GAS/GameplayAbilityBase.h"

UAbilityButton::UAbilityButton()
{

}

void UAbilityButton::OnClick()
{
	SetAbility(Id);
}


void UAbilityButton::SetAbility(int AbilityIndex)
{

	AExtendedControllerBase* ControllerBase = Cast<AExtendedControllerBase>(GetWorld()->GetFirstPlayerController());

	if(!ControllerBase) return;

	int Index = ControllerBase->CurrentUnitWidgetIndex;

	if (!ControllerBase->SelectedUnits.IsValidIndex(Index)) return;
	
	AUnitBase* Unit = ControllerBase->SelectedUnits[Index];
	
	if (!Unit)
	{
		return;
	}

	// Reject if the selected ability is disabled by flag or team key
	if (!Unit->SelectableAbilities.IsValidIndex(AbilityIndex))
	{
		return;
	}
	if (TSubclassOf<UGameplayAbilityBase> SelClass = Unit->SelectableAbilities[AbilityIndex])
	{
		if (UGameplayAbilityBase* AbilityCDO = SelClass->GetDefaultObject<UGameplayAbilityBase>())
		{
			bool bIsDisabled = AbilityCDO->bDisabled;
			bool bForceEnabled = false;
			if (!AbilityCDO->AbilityKey.IsEmpty())
			{
				bIsDisabled = bIsDisabled || UGameplayAbilityBase::IsAbilityKeyDisabledForTeam(AbilityCDO->AbilityKey, Unit->TeamId);
				bForceEnabled = UGameplayAbilityBase::IsAbilityKeyForceEnabledForTeam(AbilityCDO->AbilityKey, Unit->TeamId);
			}
			if (bIsDisabled && !bForceEnabled)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[AbilityButton] Blocked selection: Ability='%s' Key='%s' TeamId=%d bCDO_Disabled=%s bKeyDisabled=%s bForceEnabled=%s"),
					*GetNameSafe(AbilityCDO), *AbilityCDO->AbilityKey, Unit->TeamId,
					AbilityCDO->bDisabled ? TEXT("true") : TEXT("false"),
					UGameplayAbilityBase::IsAbilityKeyDisabledForTeam(AbilityCDO->AbilityKey, Unit->TeamId) ? TEXT("true") : TEXT("false"),
					bForceEnabled ? TEXT("true") : TEXT("false"));
				return; // do not add/activate when disabled without force
			}
		}
	}

	// Pick which array to modify, as a reference
	TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities =
		(ControllerBase->AbilityArrayIndex == 1) ? Unit->SecondAbilities :
		(ControllerBase->AbilityArrayIndex == 2) ? Unit->ThirdAbilities  :
		(ControllerBase->AbilityArrayIndex == 3) ? Unit->FourthAbilities :
							   Unit->DefaultAbilities;


	Abilities.Emplace(Unit->SelectableAbilities[AbilityIndex]);

}

