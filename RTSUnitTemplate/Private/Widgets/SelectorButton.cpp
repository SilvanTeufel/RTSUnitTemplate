// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/SelectorButton.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Widgets/UnitWidgetSelector.h"
#include "GameFramework/PlayerController.h"

USelectorButton::USelectorButton()
{

}

void USelectorButton::OnClick()
{
	SetUnitSelectorId(Id);
}


void USelectorButton::SetUnitSelectorId(int newID)
{

	AControllerBase* ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());

	if(!ControllerBase) return;


	ControllerBase->SetWidgets(newID);

	UUnitWidgetSelector* SelectorR = Cast<UUnitWidgetSelector>(Selector);
	
	if(SelectorR && ControllerBase->SelectedUnits[newID])
	{
		SelectorR->SetButtonColours(newID);
		FString CharacterName = ControllerBase->SelectedUnits[newID]->Name + " / " + FString::FromInt(newID);

		if (SelectorR->Name)
		{
			SelectorR->Name->SetText(FText::FromString(CharacterName));
		}
	}
	
	if(SelectUnit)
	{
		ControllerBase->SelectUnit(newID);
	}
	
}

