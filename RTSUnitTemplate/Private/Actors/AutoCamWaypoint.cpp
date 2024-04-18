// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/AutoCamWaypoint.h"
#include "Controller/ControllerBase.h"
#include "Characters/Camera/CameraBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AAutoCamWaypoint::AAutoCamWaypoint()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));
	SetRootComponent(Root);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger Box"));
	BoxComponent->SetupAttachment(GetRootComponent());
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AAutoCamWaypoint::OnPlayerEnter);

}

// Called when the game starts or when spawned
void AAutoCamWaypoint::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AAutoCamWaypoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AAutoCamWaypoint::OnPlayerEnter_Implementation(UPrimitiveComponent* OverlapComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (OtherActor != nullptr) {
		ACameraBase* CameraBase = Cast<ACameraBase>(OtherActor);
		
	}
}
