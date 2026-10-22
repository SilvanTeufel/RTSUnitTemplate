// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/ResourceClusterMarker.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AResourceClusterMarker::AResourceClusterMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetHidden(true);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}
