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

	// Disable navigation on visual components to avoid interference
	TopRodISM->SetCanEverAffectNavigation(false);
	BottomRodISM->SetCanEverAffectNavigation(false);
	ShieldISM->SetCanEverAffectNavigation(false);

	NavObstacleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("NavObstacleBox"));
	NavObstacleBox->SetupAttachment(WallRoot);
	// Collision for physical blocking and projectile interception, but NOT for NavMesh generation
	NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	NavObstacleBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	//NavObstacleBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	//NavObstacleBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	//NavObstacleBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	NavObstacleBox->SetCanEverAffectNavigation(false);

	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	NavModifier->SetAreaClass(nullptr); 
	NavModifier->bAutoActivate = false;
	NavModifier->SetActive(false);
	//NavModifier->SetAreaClass(UNavArea_Obstacle::StaticClass());

}

void AEnergyWall::BeginPlay()
{
	Super::BeginPlay();
}

void AEnergyWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsInitializing || bIsDespawning)
	{
		if (bIsInitializing)
		{
			// Linear scaling
			float Step = (TargetScaleY / InitializationDuration) * DeltaTime;
			CurrentScaleY = FMath::Min(CurrentScaleY + Step, TargetScaleY);
		}
		else // bIsDespawning
		{
			// Linear scaling
			float Step = (TargetScaleY / DespawnDelay) * DeltaTime;
			CurrentScaleY = FMath::Max(CurrentScaleY - Step, 0.f);
		}

		FTransform TopTransform;
		if (TopRodISM->GetInstanceTransform(0, TopTransform))
		{
			TopTransform.SetScale3D(FVector(1.f, CurrentScaleY, 1.f));
			TopRodISM->UpdateInstanceTransform(0, TopTransform, false, true, true);
		}

		FTransform BottomTransform;
		if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
		{
			BottomTransform.SetScale3D(FVector(1.f, CurrentScaleY, 1.f));
			BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
		}
	}
}

void AEnergyWall::Multicast_InitializeWall_Implementation(ABuildingBase* BuildingA, ABuildingBase* BuildingB)
{
	if (!BuildingA || !BuildingB) return;
	
	if (NavModifier) NavModifier->SetActive(false);
	
	CachedBuildingA = BuildingA;
	CachedBuildingB = BuildingB;

	FVector LocA = CachedBuildingA->GetActorLocation();
	FVector LocB = CachedBuildingB->GetActorLocation();

	FVector Midpoint = (LocA + LocB) * 0.5f;
	float Distance2D = FVector::Dist2D(LocA, LocB);

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
	
	// Rotate wall to align its Y-axis (Length) with the vector between buildings
	FVector Direction = (LocB - LocA);
	Direction.Z = 0; 
	FRotator Rotation = Direction.Rotation() + FRotator(0, -90.f, 0.f);
	SetActorRotation(Rotation);

	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: Actor Rotation=%s"), *Rotation.ToString());

	// Setup instances for the rods and the shield
	TopRodISM->ClearInstances();
	BottomRodISM->ClearInstances();
	ShieldISM->ClearInstances();

	float NativeTopZ = TopRodISM->GetRelativeLocation().Z;
	float NativeBottomZ = BottomRodISM->GetRelativeLocation().Z;
	float NativeHeight = FMath::Max(FMath::Abs(NativeTopZ - NativeBottomZ), 1.0f);
	float NativeLength = 100.0f;
	if (ShieldISM)
	{
		if (UStaticMesh* Mesh = ShieldISM->GetStaticMesh())
		{
			NativeLength = FMath::Max(Mesh->GetBounds().BoxExtent.Y * 2.0f * ShieldISM->GetRelativeScale3D().Y, 1.0f);
		}
	}

	// Dynamic height calculation removed - using Blueprint positions
	float WallHeight = NativeHeight;

	// Use distance / NativeLength for length scaling
	float ScaleY = Distance2D / NativeLength;
	
	TargetScaleY = ScaleY;
	TargetDistance2D = Distance2D;
	TargetWallHeight = WallHeight;
	bIsInitializing = true;

	// Add instance for top rod with 0 scale at its Blueprint position
	TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 0.f, 1.f)));
	
	// Add instance for bottom rod with 0 scale at its Blueprint position
	BottomRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 0.f, 1.f)));

	// Add instance for shield plane at its Blueprint position, and hide it
	ShieldISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, ScaleY, 1.f)));
	ShieldISM->SetHiddenInGame(true);

	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnInitializationTimerComplete, InitializationDuration, false);

	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: Instances Added. ScaleY=%f, WallHeight=%f"), ScaleY, WallHeight);
	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::Multicast_InitializeWall: Timer started. Distance2D=%f, WallHeight=%f, ScaleY=%f"), Distance2D, WallHeight, ScaleY);
}

void AEnergyWall::OnInitializationTimerComplete()
{
	bIsInitializing = false;
	CurrentScaleY = TargetScaleY;

	// Final scale update
	FTransform TopTransform;
	if (TopRodISM->GetInstanceTransform(0, TopTransform))
	{
		TopTransform.SetScale3D(FVector(1.f, TargetScaleY, 1.f));
		TopRodISM->UpdateInstanceTransform(0, TopTransform, false, true, true);
		UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::OnInitializationTimerComplete: TopRod Final Scale=%s"), *TopTransform.GetScale3D().ToString());
	}

	FTransform BottomTransform;
	if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
	{
		BottomTransform.SetScale3D(FVector(1.f, TargetScaleY, 1.f));
		BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
		UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::OnInitializationTimerComplete: BottomRod Final Scale=%s"), *BottomTransform.GetScale3D().ToString());
	}

	// Make shield visible
	ShieldISM->SetHiddenInGame(false);

	RegisterObstacle(TargetDistance2D, TargetWallHeight);
}

void AEnergyWall::InitializeISMs()
{
	if (!NavObstacleBox) return;

	// Align Box Y (Length) with Actor Y (Wall Direction)
	NavObstacleBox->SetRelativeRotation(FRotator(0, 0.f, 0));

	float NativeTopZ = TopRodISM->GetRelativeLocation().Z;
	float NativeBottomZ = BottomRodISM->GetRelativeLocation().Z;
	float NativeHeight = FMath::Max(FMath::Abs(NativeTopZ - NativeBottomZ), 1.0f);
	float NativeLength = 100.0f;
	if (ShieldISM)
	{
		if (UStaticMesh* Mesh = ShieldISM->GetStaticMesh())
		{
			NativeLength = FMath::Max(Mesh->GetBounds().BoxExtent.Y * 2.0f * ShieldISM->GetRelativeScale3D().Y, 1.0f);
		}
	}

	float WallHeight = NativeHeight;

	// Update NavObstacleBox position to be centered between rods
	float MidZ = (NativeTopZ + NativeBottomZ) * 0.5f;
	NavObstacleBox->SetRelativeLocation(FVector(0, 0, MidZ));
	float PreviewScaleY = 500.f / NativeLength; // 500 units long

	// Top Rod
	if (TopRodISM)
	{
		TopRodISM->ClearInstances();
		TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, PreviewScaleY, 1.f)));
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
		ShieldISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, PreviewScaleY, 1.f)));
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

void AEnergyWall::RegisterObstacle(float Length, float Height)
{
	FVector StartPoint = CachedBuildingA ? CachedBuildingA->GetActorLocation() : FVector::ZeroVector;
	FVector EndPoint = CachedBuildingB ? CachedBuildingB->GetActorLocation() : FVector::ZeroVector;

	// Calculate a diagonal boost for the length padding to ensure no gaps at building connections
	FRotator Rotation = GetActorRotation();
	float AngleRad = FMath::DegreesToRadians(Rotation.Yaw);
	// DiagonalScale is 1.0 for axis-aligned and ~1.414 for 45-degree diagonal
	float DiagonalScale = FMath::Abs(FMath::Sin(AngleRad)) + FMath::Abs(FMath::Cos(AngleRad));
	// Map DiagonalScale [1.0, 1.414...] to [0, 1] for smoother control
	float Alpha = FMath::Clamp((DiagonalScale - 1.0f) / 0.41421356f, 0.0f, 1.0f);

	UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::RegisterObstacle: [INPUT] Length=%f, Height=%f, StartPoint=%s, EndPoint=%s, DiagonalScale=%f, Alpha=%f"), Length, Height, *StartPoint.ToString(), *EndPoint.ToString(), DiagonalScale, Alpha);

	if (NavObstacleBox)
	{
		FVector BoxExtent;
		
		// Thickness adjusted based on angle - use more generous thickness for diagonals
		BoxExtent.X = FMath::Lerp(MinThickness, MaxThickness, Alpha) * 0.5f;
		
		// Set half-extent for Height (Z) with extra padding to ensure it reaches the ground on uneven terrain
		BoxExtent.Z = (Height * 0.5f) + NavigationZPadding;
		
		// Set half-extent for the distance (Length / 2) with padding
		float CurrentPadding = FMath::Lerp(MinPadding, MaxPadding, Alpha);
		BoxExtent.Y = (Length * 0.5f) + CurrentPadding + AgentRadiusPadding;
		
		// Set box properties and ensure navigation is triggered
		NavObstacleBox->SetBoxExtent(BoxExtent, true);
		NavObstacleBox->UpdateBounds();
		NavObstacleBox->UpdateComponentToWorld();
		
		// Position and rotate relative to actor - Actor is already rotated between buildings
		float MidZ = (TopRodISM->GetRelativeLocation().Z + BottomRodISM->GetRelativeLocation().Z) * 0.5f;
		NavObstacleBox->SetRelativeLocation(FVector(0, 0, MidZ));
		NavObstacleBox->SetRelativeRotation(FRotator::ZeroRotator);
		NavObstacleBox->UpdateComponentToWorld();

		if (NavModifier)
		{
			NavModifier->SetAreaClass(UNavArea_Obstacle::StaticClass());
			NavModifier->FailsafeExtent = BoxExtent;
			NavModifier->SetActive(false);
			NavModifier->SetActive(true);
			NavModifier->RefreshNavigationModifiers();
		}
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (NavObstacleBox)
		{
			// Expand the dirty area to ensure neighboring NavMesh tiles are properly updated
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox().ExpandBy(DirtyAreaExpansion);
			UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::RegisterObstacle: [DIRTY AREA] Box Center=%s, Box Extent=%s, Box Min=%s, Box Max=%s"), *DirtyBox.GetCenter().ToString(), *DirtyBox.GetExtent().ToString(), *DirtyBox.Min.ToString(), *DirtyBox.Max.ToString());
			
			// Mark area as dirty
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);

			// Update Octree
			NavSys->UpdateNavOctreeBounds(this);
			NavSys->UpdateActorInNavOctree(*this);
		}
	}
}

void AEnergyWall::StartDespawn(AActor* DestroyedActor)
{
	if (bIsDespawning) return;
	bIsDespawning = true;
	bIsInitializing = false;
	GetWorldTimerManager().ClearTimer(InitializationTimerHandle);

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
			// Expand the dirty area to ensure neighboring NavMesh tiles are properly updated
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox().ExpandBy(DirtyAreaExpansion);
			UE_LOG(LogTemp, Warning, TEXT("AEnergyWall::StartDespawn: Removing Dirty Area. Box Center=%s, Box Extent=%s"), *DirtyBox.GetCenter().ToString(), *DirtyBox.GetExtent().ToString());
			
			// Mark area as dirty
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);
			
			// Update Octree
			NavSys->UpdateNavOctreeBounds(this);
			NavSys->UpdateActorInNavOctree(*this);
		}
	}
	
	if (HasAuthority())
	{
		SetLifeSpan(DespawnDelay);
	}
}
