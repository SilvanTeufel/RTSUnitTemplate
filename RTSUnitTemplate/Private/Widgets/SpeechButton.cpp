// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/SpeechButton.h"
#include "Widgets/SpeechBubble.h"
#include <Components/Button.h>
#include <Components/TextBlock.h>

USpeechButton::USpeechButton()
{
	OnClicked.AddUniqueDynamic(this, &USpeechButton::OnClick);
	CallbackTableDelegate.AddUniqueDynamic(this, &USpeechButton::SetTableDataId);
}

void USpeechButton::OnClick()
{
	CallbackTableDelegate.Broadcast(New_Text_Id);
}


void USpeechButton::SetTableDataId(int newTextID)
{
	if(!SpeechBubble)
	return;
	
	USpeechBubble* Bubble = Cast<USpeechBubble>(SpeechBubble);
	
	if(!Bubble)
	return;

	if(Id)Bubble->PlayButtonSound(Id);
	Bubble->SetTableDataId(newTextID);
}