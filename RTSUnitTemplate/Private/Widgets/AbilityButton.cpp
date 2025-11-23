// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/AbilityButton.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Widgets/UnitWidgetSelector.h"
#include "GameFramework/PlayerController.h"
#include "GAS/GameplayAbilityBase.h"
#include "AbilitySystemComponent.h"

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
			bool bCDO_Disabled = AbilityCDO->bDisabled;
			bool bTeamForceEnabled = false;
			bool bOwnerForceEnabled = false;
			bool bTeamKeyDisabled = false;
			bool bOwnerKeyDisabled = false;
			const FString RawKey = AbilityCDO->AbilityKey;
			FString NormalizedKey = RawKey;
			NormalizedKey.TrimStartAndEndInline();
			NormalizedKey = NormalizedKey.ToLower();
			if (!NormalizedKey.IsEmpty() && NormalizedKey != TEXT("none"))
			{
				bTeamKeyDisabled = UGameplayAbilityBase::IsAbilityKeyDisabledForTeam(RawKey, Unit->TeamId);
				bTeamForceEnabled = UGameplayAbilityBase::IsAbilityKeyForceEnabledForTeam(RawKey, Unit->TeamId);
				if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
				{
					bOwnerKeyDisabled = UGameplayAbilityBase::IsAbilityKeyDisabledForOwner(ASC, RawKey);
					bOwnerForceEnabled = UGameplayAbilityBase::IsAbilityKeyForceEnabledForOwner(ASC, RawKey);
				}
			}
			const bool bAnyForceEnabled = bTeamForceEnabled || bOwnerForceEnabled;
			const bool bAnyDisabled = bCDO_Disabled || bTeamKeyDisabled || bOwnerKeyDisabled;
			if (bAnyDisabled && !bAnyForceEnabled)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[AbilityButton] Blocked selection: Ability='%s' Key='%s' TeamId=%d bCDO_Disabled=%s bTeamKeyDisabled=%s bOwnerKeyDisabled=%s bTeamForce=%s bOwnerForce=%s"),
					*GetNameSafe(AbilityCDO), *RawKey, Unit->TeamId,
					bCDO_Disabled ? TEXT("true") : TEXT("false"),
					bTeamKeyDisabled ? TEXT("true") : TEXT("false"),
					bOwnerKeyDisabled ? TEXT("true") : TEXT("false"),
					bTeamForceEnabled ? TEXT("true") : TEXT("false"),
					bOwnerForceEnabled ? TEXT("true") : TEXT("false"));
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

