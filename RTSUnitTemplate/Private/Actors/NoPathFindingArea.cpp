// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/NoPathFindingArea.h"

#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Unit/PathSeekerBase.h"
// Sets default values
ANoPathFindingArea::ANoPathFindingArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ANoPathFindingArea::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ANoPathFindingArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


