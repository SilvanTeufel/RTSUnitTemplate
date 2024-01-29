// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/SelectorButton.h"
#include "Controller/ControllerBase.h"
#include "Widgets/UnitWidgetSelector.h"
#include "GameFramework/PlayerController.h"

USelectorButton::USelectorButton()
{
	OnClicked.AddUniqueDynamic(this, &USelectorButton::OnClick);
	CallbackIdDelegate.AddUniqueDynamic(this, &USelectorButton::SetUnitSelectorId);
}

void USelectorButton::OnClick()
{
	CallbackIdDelegate.Broadcast(Id);
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

