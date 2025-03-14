// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/AbilityIndicator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

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
	bReplicates = true;
}

// Called when the game starts or when spawned
void AAbilityIndicator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AAbilityIndicator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAbilityIndicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityIndicator, TeamId);
}

// Sets the position of the indicator
void AAbilityIndicator::SetIndicatorLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}
