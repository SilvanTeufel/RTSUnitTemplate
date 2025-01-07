// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/AbilityButton.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Widgets/UnitWidgetSelector.h"
#include "GameFramework/PlayerController.h"

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

	// Pick which array to modify, as a reference
	TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities =
		(ControllerBase->AbilityArrayIndex == 1) ? Unit->SecondAbilities :
		(ControllerBase->AbilityArrayIndex == 2) ? Unit->ThirdAbilities  :
		(ControllerBase->AbilityArrayIndex == 3) ? Unit->FourthAbilities :
												   Unit->DefaultAbilities;


	Abilities.Emplace(Unit->SelectableAbilities[AbilityIndex]);

}

