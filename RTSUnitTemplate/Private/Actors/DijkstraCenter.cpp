// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/DijkstraCenter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"

// Sets default values
ADijkstraCenter::ADijkstraCenter()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	//SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // QueryAndPhysics
	Mesh->SetupAttachment(RootComponent);
	//Mesh->SetMaterial(0, Material);
	Mesh->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
}

// Called when the game starts or when spawned
void ADijkstraCenter::BeginPlay()
{
	Super::BeginPlay();
	
}


