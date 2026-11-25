// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AbilityIndicator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Characters/Unit/GASUnit.h"

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

	AimLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimLine"));
	AimLine->SetupAttachment(SceneRoot);
	AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimLine->SetCastShadow(false);
	AimLine->SetUsingAbsoluteLocation(true);
	AimLine->SetUsingAbsoluteRotation(true);
	AimLine->SetUsingAbsoluteScale(true);
	AimLine->SetHiddenInGame(true);
	
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
	if (AimLine)
	{
		if (AimLineMesh)
		{
			AimLine->SetStaticMesh(AimLineMesh);
		}
		if (AimLineMaterial)
		{
			AimLine->SetMaterial(0, AimLineMaterial);
		}
	}
}

// Called every frame
void AAbilityIndicator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateAimLine();
	UpdateAnchoredMesh();
}

void AAbilityIndicator::UpdateAnchoredMesh()
{
	if (!bAnchorMeshAtCaster || !IndicatorMesh)
	{
		return;
	}
	const AGASUnit* Caster = nullptr;
	for (const AGASUnit* Unit : AssociatedUnits)
	{
		if (IsValid(Unit))
		{
			Caster = Unit;
			break;
		}
	}
	if (!Caster)
	{
		return;
	}
	const FVector Mouse = GetActorLocation();
	const FVector At = Caster->GetMassActorLocation();
	const FVector Dir(Mouse.X - At.X, Mouse.Y - At.Y, 0.f);
	const float Yaw = Dir.IsNearlyZero(1.f) ? Caster->GetActorRotation().Yaw : Dir.Rotation().Yaw;
	IndicatorMesh->SetWorldLocationAndRotation(FVector(At.X, At.Y, Mouse.Z + AnchorHeight), FRotator(0.f, Yaw, 0.f));
}

void AAbilityIndicator::UpdateAimLine()
{
	if (!AimLine)
	{
		return;
	}
	const AGASUnit* Caster = nullptr;
	for (const AGASUnit* Unit : AssociatedUnits)
	{
		if (IsValid(Unit))
		{
			Caster = Unit;
			break;
		}
	}
	const bool bIndicatorShown = IndicatorMesh && IndicatorMesh->IsVisible() && !IndicatorMesh->bHiddenInGame && !IsHidden();
	if (!bShowAimLine || !Caster || !bIndicatorShown || !AimLine->GetStaticMesh())
	{
		AimLine->SetHiddenInGame(true);
		return;
	}

	const FVector End = GetActorLocation();
	FVector Start = Caster->GetMassActorLocation();
	FVector Dir = FVector(End.X - Start.X, End.Y - Start.Y, 0.f);
	const float Full = Dir.Size();
	if (Full < AimLineStartOffset + 10.f)
	{
		AimLine->SetHiddenInGame(true);
		return;
	}
	Dir /= Full;
	Start += Dir * AimLineStartOffset;
	const float Length = Full - AimLineStartOffset;
	const FVector Mid = FVector((Start.X + End.X) * 0.5f, (Start.Y + End.Y) * 0.5f, End.Z + AimLineHeight);

	// The mesh is 100 x 100 units (engine plane) - scale it to the length along X and the width along Y.
	const FBoxSphereBounds Bounds = AimLine->GetStaticMesh()->GetBounds();
	const float MeshX = FMath::Max(Bounds.BoxExtent.X * 2.f, 1.f);
	const float MeshY = FMath::Max(Bounds.BoxExtent.Y * 2.f, 1.f);
	AimLine->SetWorldLocationAndRotation(Mid, FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	AimLine->SetWorldScale3D(FVector(Length / MeshX, AimLineWidth / MeshY, 1.f));
	AimLine->SetHiddenInGame(false);
}

// Sets the position of the indicator
void AAbilityIndicator::SetIndicatorLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}
