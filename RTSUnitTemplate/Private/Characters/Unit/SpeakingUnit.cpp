// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/SpeakingUnit.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Sound\SoundCue.h"

ASpeakingUnit::ASpeakingUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	SpeechBubbleWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("SpeechBubble"));
	SpeechBubbleWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SpeechBubbleWidgetComp->SetRelativeLocation(SpeechBubbleWidgetCompLocation, false, 0, ETeleportType::None);
	
}

// Called when the game starts or when spawned
void ASpeakingUnit::BeginPlay()
{
	Super::BeginPlay();
	SetSpeechWidgetText();

}

void ASpeakingUnit::PlaySoundOnce(float Timer, UAudioComponent*& Component, USoundBase* Sound, float Volume)
{
	if(Timer == 0.f)
	{
		if(Component)
			Component->Stop();
				
		Component = UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation(), GetActorRotation(), Volume);
	}
}

// Called every frame
void ASpeakingUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if(SpeechBubble && SpeechBubble->WidgetIndex == 2)
	{

		PlaySoundOnce(SpeechBubble->SpeechSoundTimer, SpeechBubble->SpeechSoundComponent, SpeechBubble->SpeechSound, SpeechVolume );
		SpeechBubble->SpeechSoundTimer += DeltaTime;
		PlaySoundOnce(SpeechBubble->BackgroundSoundTimer, SpeechBubble->BackgroundSoundComponent, SpeechBubble->BackgroundSound, BackgroundVolume );
		SpeechBubble->BackgroundSoundTimer += DeltaTime;
		LockCamOnUnit = true;
	}else
	{
		LockCamOnUnit = false;
		SpeechBubble->SpeechSoundTimer = 0.f;
	}
}

void ASpeakingUnit::SetSpeechWidgetText()
{
	if (SpeechBubbleWidgetComp) {

		SpeechBubbleWidgetComp->SetRelativeLocation(SpeechBubbleWidgetCompLocation, false, 0, ETeleportType::None);
		SpeechBubble = Cast<USpeechBubble>(SpeechBubbleWidgetComp->GetUserWidgetObject());

		if (SpeechBubble) {
			SpeechBubble->SetTableDataId(Text_Id);
		}
	}
}

void ASpeakingUnit::SetSpeechWidgetLocation(FVector NewLocation)
{
	SpeechBubbleWidgetCompLocation = NewLocation;
}
