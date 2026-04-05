// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AbilityIndicator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

// Sets default values
AAbilityIndicator::AAbilityIndicator()
{
	// Set this actor to call Tick() every frame. You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create and set the root component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot); 

	// Create and attach the static mesh component
	IndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IndicatorMesh"));
	IndicatorMesh->SetupAttachment(SceneRoot);
	IndicatorMesh->SetHiddenInGame(true);

	// Disable collisions on the mesh component
	IndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// Optionally, disable collision for the entire actor
	SetActorEnableCollision(false);
	
	bReplicates = false;
	bIgnoreHoldingUnitInDistanceCheck = true;
}

// Called when the game starts or when spawned
void AAbilityIndicator::BeginPlay()
{
	Super::BeginPlay();
	// Cache original material from the mesh if not explicitly assigned
	if (IndicatorMesh && OriginalMaterial == nullptr)
	{
		OriginalMaterial = IndicatorMesh->GetMaterial(0);
	}
	IsOverlappedWithNoBuildZone = false;
}

// Called every frame
void AAbilityIndicator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Sets the position of the indicator
void AAbilityIndicator::SetIndicatorLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}
