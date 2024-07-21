// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/SpeechBubble.h"
#include <Components/TextBlock.h>
#include <Components/Button.h>
#include "Components/WidgetSwitcher.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"

void USpeechBubble::NativeConstruct()
{
	Super::NativeConstruct();
	SwitchWidgetButton->OnClicked.AddUniqueDynamic(this, &USpeechBubble::OnClick);
	CloseWidgetButton->OnClicked.AddUniqueDynamic(this, &USpeechBubble::OnClick);
	CallbackSwitchWidgetDelegate.AddUniqueDynamic(this, &USpeechBubble::SwitchWidget);
}



void USpeechBubble::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void USpeechBubble::OnClick()
{
	if(WidgetIndex == 1)
	{
		WidgetIndex = 2;
		BackgroundSoundTimer = 0.f;
	}
	else if(WidgetIndex == 2)
	{
		WidgetIndex = 0;
		
		if(BackgroundSoundComponent)
			BackgroundSoundComponent->Stop();

		if(SpeechSoundComponent)
			SpeechSoundComponent->Stop();
	}else{
		if(BackgroundSoundComponent)
			BackgroundSoundComponent->Stop();

		if(SpeechSoundComponent)
			SpeechSoundComponent->Stop();
	}
	
	CallbackSwitchWidgetDelegate.Broadcast(WidgetIndex);
}

void USpeechBubble::SwitchWidget(int Index)
{
	WidgetSwitcher->SetActiveWidgetIndex(Index);
}

void USpeechBubble::SetTableDataId(int T_Id)
{
	class UDataTable* SpeechTable_Texts = SpeechDataTable_Texts;
	class UDataTable* SpeechTable_Buttons = SpeechDataTable_Buttons;

	if (SpeechTable_Texts)
	{
		for(auto it : SpeechTable_Texts->GetRowMap())
		{
			FString Key = it.Key.ToString();
			FSpeechData_Texts* UnitSpeechData_Texts = reinterpret_cast<FSpeechData_Texts*>(it.Value);
			if(UnitSpeechData_Texts->Id == T_Id)
			{
		
				Text->SetText(UnitSpeechData_Texts->Text);
				SpeechSound = UnitSpeechData_Texts->SpeechSound;
				BlendPoint_1 = UnitSpeechData_Texts->BlendPoint_1;
				BlendPoint_2 = UnitSpeechData_Texts->BlendPoint_2;
				MaxAnimationTime = UnitSpeechData_Texts->Time;
				SpeechSoundTimer = 0.f;
				AnimationTime = 0.f;
				
				if (SpeechTable_Buttons)
				{
					int z = 1;
					for(auto itt : SpeechTable_Buttons->GetRowMap())
					{
						FSpeechData_Buttons* UnitSpeechData_Buttons = reinterpret_cast<FSpeechData_Buttons*>(itt.Value);
						if(UnitSpeechData_Buttons->Text_Id == T_Id)
						{
						
							
							if(z == 1 && Button_1)
							{
								Button_1->New_Text_Id = UnitSpeechData_Buttons->New_Text_Id;
								Button_Text_1->SetText(UnitSpeechData_Buttons->Text);
								Button_1->SpeechBubble = this;
								Button_1->Id = UnitSpeechData_Buttons->Id;
								
								z++;
							}else if(z == 2 && Button_2)
							{
								Button_2->New_Text_Id = UnitSpeechData_Buttons->New_Text_Id;
								Button_Text_2->SetText(UnitSpeechData_Buttons->Text);
								Button_2->SpeechBubble = this;
								Button_2->Id = UnitSpeechData_Buttons->Id;
								
								z++;
							}else if(z == 3 && Button_3)
							{
								Button_3->New_Text_Id = UnitSpeechData_Buttons->New_Text_Id;
								Button_Text_3->SetText(UnitSpeechData_Buttons->Text);
								Button_3->SpeechBubble = this;
								Button_3->Id = UnitSpeechData_Buttons->Id;
								
								z++;
							}else if(z == 4 && Button_4)
							{
								Button_4->New_Text_Id = UnitSpeechData_Buttons->New_Text_Id;
								Button_Text_4->SetText(UnitSpeechData_Buttons->Text);
								Button_4->SpeechBubble = this;
								Button_4->Id = UnitSpeechData_Buttons->Id;
								
								z++;
							}
						
						}
					}
				}
			
			}
		}
	}
}


void USpeechBubble::PlayButtonSound(int B_Id)
{
	class UDataTable* SpeechTable_Buttons = SpeechDataTable_Buttons;

	if (SpeechTable_Buttons)
	{
	
		for(auto itt : SpeechTable_Buttons->GetRowMap())
		{
			FSpeechData_Buttons* UnitSpeechData_Buttons = reinterpret_cast<FSpeechData_Buttons*>(itt.Value);
			if(UnitSpeechData_Buttons->Id == B_Id)
			{
				FVector Location = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
				UGameplayStatics::PlaySoundAtLocation(this, UnitSpeechData_Buttons->ButtonSound, Location, 1.f);
			}
		}
	}
}
