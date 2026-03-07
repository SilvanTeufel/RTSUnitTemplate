// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EnergyWall.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavigationSystem.h"
#include "Characters/Unit/BuildingBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/StaticMesh.h"

AEnergyWall::AEnergyWall()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	WallRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WallRoot"));
	RootComponent = WallRoot;

	TopRodISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TopRodISM"));
	TopRodISM->SetupAttachment(WallRoot);

	BottomRodISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BottomRodISM"));
	BottomRodISM->SetupAttachment(WallRoot);

	ShieldISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ShieldISM"));
	ShieldISM->SetupAttachment(WallRoot);

	NavObstacleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("NavObstacleBox"));
	NavObstacleBox->SetupAttachment(WallRoot);
	NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NavObstacleBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	NavObstacleBox->SetCanEverAffectNavigation(true);

	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	NavModifier->SetAreaClass(UNavArea_Obstacle::StaticClass());
}

void AEnergyWall::BeginPlay()
{
	Super::BeginPlay();
}

void AEnergyWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnergyWall::Multicast_InitializeWall_Implementation(ABuildingBase* BuildingA, ABuildingBase* BuildingB)
{
	if (!BuildingA || !BuildingB) return;

	CachedBuildingA = BuildingA;
	CachedBuildingB = BuildingB;

	FVector LocA = CachedBuildingA->GetActorLocation();
	FVector LocB = CachedBuildingB->GetActorLocation();

	FVector Midpoint = (LocA + LocB) * 0.5f;
	float Distance = FVector::Dist(LocA, LocB);

	// Perform ground trace at midpoint to position the wall base correctly
	FHitResult Hit;
	FVector Start = Midpoint + FVector(0, 0, 1000.f);
	FVector End = Midpoint - FVector(0, 0, 1000.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(BuildingA);
	Params.AddIgnoredActor(BuildingB);

	float GroundZ = Midpoint.Z;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		GroundZ = Hit.Location.Z;
	}

	SetActorLocation(FVector(Midpoint.X, Midpoint.Y, GroundZ));
	
	// Rotate wall to align its X-axis with the vector between buildings
	FVector Direction = (LocB - LocA);
	Direction.Z = 0; 
	FRotator Rotation = Direction.Rotation() + FRotator(0, -90.f, 0.f);
	SetActorRotation(Rotation);

	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: Actor Rotation=%s"), *Rotation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: TopRodISM Mesh=%s, Relative Rotation=%s"), TopRodISM->GetStaticMesh() ? *TopRodISM->GetStaticMesh()->GetName() : TEXT("None"), *TopRodISM->GetRelativeRotation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: BottomRodISM Mesh=%s, Relative Rotation=%s"), BottomRodISM->GetStaticMesh() ? *BottomRodISM->GetStaticMesh()->GetName() : TEXT("None"), *BottomRodISM->GetRelativeRotation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: ShieldISM Mesh=%s, Relative Rotation=%s"), ShieldISM->GetStaticMesh() ? *ShieldISM->GetStaticMesh()->GetName() : TEXT("None"), *ShieldISM->GetRelativeRotation().ToString());

	// Setup instances for the rods and the shield
	TopRodISM->ClearInstances();
	BottomRodISM->ClearInstances();
	ShieldISM->ClearInstances();

	FVector BoxExtent = NavObstacleBox->GetUnscaledBoxExtent();
	float NativeLength = BoxExtent.Y * 2.0f;
	float WallHeight = BoxExtent.Z * 2.0f;

	// Use distance / NativeLength for length scaling
	float ScaleY = Distance / NativeLength;
	
	// Add instance for top rod (offset by WallHeight)
	TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, WallHeight), FVector(1.f, ScaleY, 1.f)));
	
	// Add instance for bottom rod
	BottomRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, ScaleY, 1.f)));

	// Add instance for shield plane (vertical, centered)
	// We dont need to ScaleZ (set to 1.f)
	float ScaleZ = 1.f;
	// Fixed Scale: When rotated 90 Pitch, Mesh X is height (ScaleZ), Mesh Y is length (ScaleY)
	ShieldISM->AddInstance(FTransform(FRotator(90.f, 0, 0), FVector(0, 0, WallHeight * 0.5f), FVector(ScaleZ, ScaleY, 1.f)));

	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: Shield Instance Transform Applied. Rotation=90 Pitch, Scale=(%f, %f, %f)"), ScaleZ, ScaleY, 1.f);
	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: Transforms updated. Distance=%f, ScaleY=%f, ScaleZ=%f"), Distance, ScaleY, ScaleZ);

	RegisterObstacle(Distance);
}

void AEnergyWall::InitializeISMs()
{
	if (!NavObstacleBox) return;

	// Align Box Y (Length) with Actor X
	NavObstacleBox->SetRelativeRotation(FRotator(0, 0.f, 0));

	FVector BoxExtent = NavObstacleBox->GetUnscaledBoxExtent();
	float NativeLength = BoxExtent.Y * 2.0f;
	float WallHeight = BoxExtent.Z * 2.0f;

	// Update NavObstacleBox position
	NavObstacleBox->SetRelativeLocation(FVector(0, 0, BoxExtent.Z));
	float PreviewScaleY = 500.f / NativeLength; // 500 units long
	float PreviewScaleZ = 1.0f;

	// Top Rod
	if (TopRodISM)
	{
		TopRodISM->ClearInstances();
		TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, WallHeight), FVector(1.f, PreviewScaleY, 1.f)));
	}

	// Bottom Rod
	if (BottomRodISM)
	{
		BottomRodISM->ClearInstances();
		BottomRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, PreviewScaleY, 1.f)));
	}

	// Shield
	if (ShieldISM)
	{
		ShieldISM->ClearInstances();
		ShieldISM->AddInstance(FTransform(FRotator(90.f, 0, 0), FVector(0, 0, WallHeight * 0.5f), FVector(PreviewScaleZ, PreviewScaleY, 1.f)));
	}
}

int32 AEnergyWall::InitializeAdditionalISM(UInstancedStaticMeshComponent* InISMComponent)
{
	if (!InISMComponent || !InISMComponent->GetStaticMesh())
	{
		return INDEX_NONE;
	}

	const FTransform LocalIdentityTransform = FTransform::Identity;
	if (InISMComponent->GetInstanceCount() == 0)
	{
		return InISMComponent->AddInstance(LocalIdentityTransform, false);
	}
	else
	{
		InISMComponent->UpdateInstanceTransform(0, LocalIdentityTransform, false, true, false);
		return 0;
	}
}

void AEnergyWall::RegisterObstacle(float Length)
{
	if (NavObstacleBox)
	{
		// Get BP set unscaled extents to preserve width (X) and height (Z)
		FVector BoxExtent = NavObstacleBox->GetUnscaledBoxExtent();
		
		// Ensure a minimum thickness (X) and height (Z) for reliable navigation
		BoxExtent.X = FMath::Max(BoxExtent.X, 30.0f);
		BoxExtent.Z = FMath::Max(BoxExtent.Z, 50.0f);
		
		// Set half-extent for the distance (Length / 2)
		BoxExtent.Y = Length * 0.5f; 
		
		NavObstacleBox->SetBoxExtent(BoxExtent, false);
		NavObstacleBox->SetRelativeLocation(FVector(0, 0, BoxExtent.Z));
		NavObstacleBox->UpdateComponentToWorld();

		if (NavModifier)
		{
			NavModifier->FailsafeExtent = BoxExtent;
			NavModifier->RefreshNavigationModifiers();
			UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::RegisterObstacle: NavModifier FailsafeExtent=%s"), *NavModifier->FailsafeExtent.ToString());
		}
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (NavObstacleBox)
		{
			// Notify the navigation system that this actor's navigation data has changed
			NavSys->UpdateNavOctreeBounds(this);

			FBox DirtyBox = NavObstacleBox->Bounds.GetBox();
			UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::RegisterObstacle: Adding Dirty Area. Box Center=%s, Box Extent=%s"), *DirtyBox.GetCenter().ToString(), *DirtyBox.GetExtent().ToString());
			// Dirty the actual component bounds of the box
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);
		}
	}
}

void AEnergyWall::StartDespawn(AActor* DestroyedActor)
{
	if (bIsDespawning) return;
	bIsDespawning = true;

	// Unbind to prevent multiple calls
	if (CachedBuildingA) CachedBuildingA->OnDestroyed.RemoveAll(this);
	if (CachedBuildingB) CachedBuildingB->OnDestroyed.RemoveAll(this);

	// Create dynamic material instances and set the start time for the despawn effect
	TArray<UInstancedStaticMeshComponent*> Components = { ShieldISM, TopRodISM, BottomRodISM };
	for (UInstancedStaticMeshComponent* Comp : Components)
	{
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
		{
			UMaterialInterface* Mat = Comp->GetMaterial(i);
			if (Mat)
			{
				UMaterialInstanceDynamic* DynamicMat = Comp->CreateDynamicMaterialInstance(i, Mat);
				if (DynamicMat)
				{
					DynamicMat->SetScalarParameterValue(DespawnStartTimeParameterName, GetWorld()->GetTimeSeconds());
				}
			}
		}
	}

	// Update navigation to mark the area as clear
	if (NavObstacleBox)
	{
		NavObstacleBox->SetCanEverAffectNavigation(false);
	}

	if (NavModifier)
	{
		NavModifier->SetAreaClass(nullptr);
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (NavObstacleBox)
		{
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox();
			UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::StartDespawn: Removing Dirty Area. Box Center=%s, Box Extent=%s"), *DirtyBox.GetCenter().ToString(), *DirtyBox.GetExtent().ToString());
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);
		}
	}
	
	if (HasAuthority())
	{
		SetLifeSpan(DespawnDelay);
	}
}
