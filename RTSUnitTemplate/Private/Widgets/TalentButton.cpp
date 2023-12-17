#include "Widgets/TalentButton.h"

UTalentButton::UTalentButton()
{
	OnClicked.AddDynamic(this, &UTalentButton::OnClick);
}

void UTalentButton::OnClick()
{
	OnTalentButtonClicked.Broadcast(ButtonId);
}

void UTalentButton::SetupButton(int32 InButtonId)
{
	ButtonId = InButtonId;
}