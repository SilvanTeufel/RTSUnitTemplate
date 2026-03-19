// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/EnergyWall.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
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
	NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavObstacleBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	NavObstacleBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	NavObstacleBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	NavObstacleBox->SetCanEverAffectNavigation(true);
	NavObstacleBox->OnComponentBeginOverlap.AddDynamic(this, &AEnergyWall::OnOverlapBegin);

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

	if (!bIsInitialized && CachedBuildingA && CachedBuildingB)
	{
		UpdateVisibility();
		if (bIsVisibleByFoW)
		{
			InitializeWallInternal();
		}
	}

	if (bIsInitialized && (bIsInitializing || bIsDespawning))
	{
		float Alpha = 0.f;
		if (bIsInitializing)
		{
			float Elapsed = GetWorldTimerManager().GetTimerElapsed(InitializationTimerHandle);
			if (Elapsed < 0.f) Elapsed = InitializationDuration;
			float TotalInitTime = FMath::Max(InitializationDuration, 0.001f);
			Alpha = FMath::Clamp(Elapsed / TotalInitTime, 0.f, 1.f);

			if (Alpha <= 0.5f)
			{
				CurrentScaleY = TargetScaleY * (Alpha * 2.f);
			}
			else
			{
				CurrentScaleY = TargetScaleY;
			}
		}
		else // bIsDespawning
		{
			float Elapsed = GetWorldTimerManager().GetTimerElapsed(InitializationTimerHandle);
			float TotalDespawnTime = FMath::Max(DespawnDelay, 0.001f);
			
			// Fallback to LifeSpan if timer is not active (e.g. real destruction)
			if (Elapsed < 0.f && GetLifeSpan() > 0.f)
			{
				float Remaining = GetLifeSpan();
				Alpha = 1.f - FMath::Clamp(Remaining / TotalDespawnTime, 0.f, 1.f);
			}
			else
			{
				Alpha = FMath::Clamp(Elapsed / TotalDespawnTime, 0.f, 1.f);
			}

			if (Alpha >= 0.5f)
			{
				float ScalingAlpha = (Alpha - 0.5f) * 2.f;
				CurrentScaleY = TargetScaleY * (1.f - ScalingAlpha);
			}
			else
			{
				CurrentScaleY = TargetScaleY;
			}
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

		FTransform ShieldTransform;
		if (ShieldISM->GetInstanceTransform(0, ShieldTransform))
		{
			ShieldTransform.SetScale3D(FVector(1.f, CurrentScaleY, 1.f));
			ShieldISM->UpdateInstanceTransform(0, ShieldTransform, false, true, true);
		}

		// Handle Shield Visibility (Flickering or hidden)
		if (!bIsVisibleByFoW)
		{
			ShieldISM->SetHiddenInGame(true);
		}
		else if (bIsInitializing)
		{
			if (Alpha > 0.5f)
			{
				if (bFlickerOnInitialize)
				{
					float FlickerAlpha = (Alpha - 0.5f) * 2.f;
					float VisibilityProb = FMath::Lerp(0.1f, 1.0f, FlickerAlpha);
					float Frequency = FMath::Lerp(25.f, 5.f, FlickerAlpha);
					const float SwitchProb = ShieldISM->bHiddenInGame ? (Frequency * DeltaTime * VisibilityProb) : (Frequency * DeltaTime * (1.f - VisibilityProb));
					if (FMath::FRand() < SwitchProb)
					{
						ShieldISM->SetHiddenInGame(!ShieldISM->bHiddenInGame);
					}
				}
				else
				{
					ShieldISM->SetHiddenInGame(false);
				}
			}
			else
			{
				ShieldISM->SetHiddenInGame(true);
			}
		}
		else if (bIsDespawning)
		{
			if (Alpha <= 0.5f)
			{
				if (bFlickerOnDespawn)
				{
					float FlickerAlpha = Alpha * 2.f;
					float VisibilityProb = FMath::Lerp(0.9f, 0.0f, FlickerAlpha);
					float Frequency = FMath::Lerp(25.f, 5.f, FlickerAlpha);
					const float SwitchProb = ShieldISM->bHiddenInGame ? (Frequency * DeltaTime * VisibilityProb) : (Frequency * DeltaTime * (1.f - VisibilityProb));
					if (FMath::FRand() < SwitchProb)
					{
						ShieldISM->SetHiddenInGame(!ShieldISM->bHiddenInGame);
					}
				}
				else
				{
					ShieldISM->SetHiddenInGame(false);
				}
			}
			else
			{
				ShieldISM->SetHiddenInGame(true);
			}
		}
	}
}

void AEnergyWall::Multicast_InitializeWall_Implementation(ABuildingBase* BuildingA, ABuildingBase* BuildingB)
{
	CachedBuildingA = BuildingA;
	CachedBuildingB = BuildingB;

	if (CachedBuildingA && CachedBuildingB)
	{
		InitializeWallInternal();
	}
}

void AEnergyWall::UpdateWallTransformAndDimensions()
{
	if (!CachedBuildingA || !CachedBuildingB) return;

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
	Params.AddIgnoredActor(CachedBuildingA);
	Params.AddIgnoredActor(CachedBuildingB);

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

	TargetScaleY = Distance2D / NativeLength;
	TargetDistance2D = Distance2D;
	TargetWallHeight = NativeHeight;
}

void AEnergyWall::InitializeWallInternal()
{
	if (bIsInitialized || !CachedBuildingA || !CachedBuildingB) return;

	if (NavModifier) NavModifier->SetActive(false);

	UpdateWallTransformAndDimensions();

	// Setup instances for the rods and the shield
	TopRodISM->ClearInstances();
	BottomRodISM->ClearInstances();
	ShieldISM->ClearInstances();

	bIsInitializing = true;
	bIsInitialized = true;

	// Add instance for top rod with 0 scale at its Blueprint position
	TopRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 0.f, 1.f)));
	
	// Add instance for bottom rod with 0 scale at its Blueprint position
	BottomRodISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, 0.f, 1.f)));

	// Add instance for shield plane at its Blueprint position, and hide it
	ShieldISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.f, TargetScaleY, 1.f)));
	ShieldISM->SetHiddenInGame(true);

	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnInitializationTimerComplete, InitializationDuration, false);

	UpdateVisibility();
}

void AEnergyWall::UpdateVisibility()
{
	bIsVisibleByFoW = false;

	if (CachedBuildingA && (CachedBuildingA->IsMyTeam || CachedBuildingA->IsVisibleEnemy))
	{
		bIsVisibleByFoW = true;
	}
	else if (CachedBuildingB && (CachedBuildingB->IsMyTeam || CachedBuildingB->IsVisibleEnemy))
	{
		bIsVisibleByFoW = true;
	}

	if (TopRodISM) TopRodISM->SetHiddenInGame(!bIsVisibleByFoW);
	if (BottomRodISM) BottomRodISM->SetHiddenInGame(!bIsVisibleByFoW);
	
	if (ShieldISM)
	{
		if (!bIsVisibleByFoW || bIsDeactivated)
		{
			ShieldISM->SetHiddenInGame(true);
		}
		else if (!bIsInitializing && !bIsDespawning)
		{
			ShieldISM->SetHiddenInGame(false);
		}
	}
}

void AEnergyWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnergyWall, CachedBuildingA);
	DOREPLIFETIME(AEnergyWall, CachedBuildingB);
	DOREPLIFETIME(AEnergyWall, bIsDeactivated);
	DOREPLIFETIME(AEnergyWall, TeamId);
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
	}

	FTransform BottomTransform;
	if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
	{
		BottomTransform.SetScale3D(FVector(1.f, TargetScaleY, 1.f));
		BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
	}

	// Update visibility based on FoW
	UpdateVisibility();

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

void AEnergyWall::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
	if (Unit)
	{
		UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
		if (ASC)
		{
			TSubclassOf<UGameplayEffect> EffectToApply = (Unit->TeamId == this->TeamId) ? FriendlyEffectClass : EnemyEffectClass;
			if (EffectToApply)
			{
				FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
				EffectContext.AddInstigator(this, this);

				FGameplayEffectSpecHandle NewHandle = ASC->MakeOutgoingSpec(EffectToApply, 1.0f, EffectContext);
				if (NewHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*NewHandle.Data.Get());
				}
			}
		}
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

	if (NavObstacleBox)
	{
		// Toggle collision instead of CanEverAffectNavigation for more reliable updates
		NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		NavObstacleBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

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
			NavModifier->SetActive(true);
		}
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (NavObstacleBox)
		{
			// Expand the dirty area to ensure neighboring NavMesh tiles are properly updated
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox().ExpandBy(DirtyAreaExpansion);
			
			// Mark area as dirty
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);

			// Update Octree
			NavSys->UpdateNavOctreeBounds(this);
			NavSys->UpdateActorInNavOctree(*this);

			// Refresh modifiers after the octree update
			if (NavModifier)
			{
				NavModifier->RefreshNavigationModifiers();
			}
		}
	}
}

void AEnergyWall::DeactivateNavigation()
{
	if (NavObstacleBox)
	{
		// Use collision toggling for dynamic navigation state changes
		NavObstacleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (NavModifier)
	{
		NavModifier->SetAreaClass(nullptr);
		NavModifier->SetActive(false);
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		// Force refresh of the octree and then modifiers
		NavSys->UpdateNavOctreeBounds(this);
		NavSys->UpdateActorInNavOctree(*this);

		if (NavModifier)
		{
			NavModifier->RefreshNavigationModifiers();
		}

		if (NavObstacleBox)
		{
			FBox DirtyBox = NavObstacleBox->Bounds.GetBox().ExpandBy(DirtyAreaExpansion);
			NavSys->AddDirtyArea(DirtyBox, ENavigationDirtyFlag::All);
		}
	}
}

void AEnergyWall::ApplyDespawnEffects()
{
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
}

void AEnergyWall::ActivateNavigation()
{
	RegisterObstacle(TargetDistance2D, TargetWallHeight);
}

void AEnergyWall::Multicast_DeactivateWall_Implementation()
{
	if (bIsDeactivated || bIsDespawning) return;

	bIsDespawning = true;
	bIsInitializing = false;
	bIsDeactivated = true;

	ApplyDespawnEffects();
	DeactivateNavigation();

	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnDeactivationTimerComplete, DespawnDelay, false);
}

void AEnergyWall::Multicast_ActivateWall_Implementation()
{
	if (!bIsDeactivated && !bIsDespawning) return;

	UpdateWallTransformAndDimensions();
	
	bIsInitializing = true;
	bIsDespawning = false;
	bIsDeactivated = false;

	GetWorldTimerManager().SetTimer(InitializationTimerHandle, this, &AEnergyWall::OnInitializationTimerComplete, InitializationDuration, false);

	UpdateVisibility();
}

void AEnergyWall::OnDeactivationTimerComplete()
{
	bIsDespawning = false;
	CurrentScaleY = 0.f;

	// Final scale update to ensure it's hidden
	FTransform TopTransform;
	if (TopRodISM->GetInstanceTransform(0, TopTransform))
	{
		TopTransform.SetScale3D(FVector(1.f, 0.f, 1.f));
		TopRodISM->UpdateInstanceTransform(0, TopTransform, false, true, true);
	}

	FTransform BottomTransform;
	if (BottomRodISM->GetInstanceTransform(0, BottomTransform))
	{
		BottomTransform.SetScale3D(FVector(1.f, 0.f, 1.f));
		BottomRodISM->UpdateInstanceTransform(0, BottomTransform, false, true, true);
	}

	UpdateVisibility();
}

void AEnergyWall::StartDespawn(AActor* DestroyedActor)
{
	if (bIsDespawning)
	{
		if (GetLifeSpan() > 0.0f)
		{
			return;
		}
	}
	
	bIsDespawning = true;
	bIsInitializing = false;
	GetWorldTimerManager().ClearTimer(InitializationTimerHandle);

	if (CachedBuildingA) CachedBuildingA->OnDestroyed.RemoveAll(this);
	if (CachedBuildingB) CachedBuildingB->OnDestroyed.RemoveAll(this);

	ApplyDespawnEffects();
	DeactivateNavigation();
	
	if (HasAuthority())
	{
		SetLifeSpan(DespawnDelay);
	}
}
