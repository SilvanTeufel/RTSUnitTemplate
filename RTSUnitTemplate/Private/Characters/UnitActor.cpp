// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/UnitActor.h"
#include "AbilitySystemComponent.h"

// Sets default values
AUnitActor::AUnitActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root Component = Capsule (works for both modes)
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(34.f, 88.f);
	RootComponent = CapsuleComponent;

	// Skeletal Mesh (only visible when in Skeletal mode)
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(CapsuleComponent);
	SkeletalMesh->SetVisibility(false);

	// ISM Component (optional — only visible in ISM mode)
	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
	ISMComponent->SetupAttachment(CapsuleComponent);
	ISMComponent->SetVisibility(false);

	// Unit Logic (shared gameplay logic)

}

// Called when the game starts or when spawned
void AUnitActor::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeUnitMode();


}

// Called every frame
void AUnitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}



void AUnitActor::InitializeUnitMode()
{
	if (bUseSkeletalMovement)
	{
		SkeletalMesh->SetVisibility(true);
		ISMComponent->SetVisibility(false);
	}
	else
	{
		SkeletalMesh->SetVisibility(false);
		ISMComponent->SetVisibility(true);
	}
}