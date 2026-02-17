// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AreaDecalComponent.h" // Adjust path if necessary

#include "Characters/Unit/UnitBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Characters/Unit/BuildingBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "VT/RuntimeVirtualTexture.h"
#include "UObject/ConstructorHelpers.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/RSVirtualTextureActor.h"

UAreaDecalComponent::UAreaDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f; 
	SetIsReplicatedByDefault(true);

	// Initial state is inactive.
	SetVisibility(false);
	SetHiddenInGame(true);
	CurrentMaterial = nullptr;
	CurrentDecalRadius = 0.f;
	
	// Rotate the decal to project downwards onto the ground.
	SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	// Default size, will be overridden by activation.
	DecalSize = FVector(100.f, 100.f, 100.f); 

	// RVT Writer Setup
	RVTWriterComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RVTWriterComponent"));
	RVTWriterComponent->SetupAttachment(this);
	RVTWriterComponent->SetCastShadow(false);
	RVTWriterComponent->SetReceivesDecals(false);
	RVTWriterComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RVTWriterComponent->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;
	RVTWriterComponent->bRenderInMainPass = false;
	RVTWriterComponent->SetHiddenInGame(true);
	RVTWriterComponent->SetVisibility(false);
	RVTWriterComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		RVTWriterComponent->SetStaticMesh(PlaneMeshAsset.Object);
	}
}

void UAreaDecalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Register properties for replication.
	DOREPLIFETIME(UAreaDecalComponent, CurrentMaterial);
	DOREPLIFETIME(UAreaDecalComponent, CurrentDecalColor);
	DOREPLIFETIME(UAreaDecalComponent, CurrentDecalRadius);
	DOREPLIFETIME(UAreaDecalComponent, bDecalIsVisible);
	DOREPLIFETIME(UAreaDecalComponent, bUseRuntimeVirtualTexture);
}

void UAreaDecalComponent::BeginPlay()
{
	Super::BeginPlay();
	SetHiddenInGame(true);

	if (RVTWriterComponent)
	{
		// Re-attach to parent to avoid Decal rotation inherited from this component
		USceneComponent* AttachTarget = GetAttachParent() ? GetAttachParent() : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
		if (AttachTarget && AttachTarget != this)
		{
			RVTWriterComponent->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform);
			RVTWriterComponent->SetRelativeLocation(FVector(0.f, 0.f, 2.0f));
			RVTWriterComponent->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
	
	if (bUseRuntimeVirtualTexture)
	{
		UE_LOG(LogTemp, Log, TEXT("UAreaDecalComponent::BeginPlay - RVT mode active for '%s'"), *GetOwner()->GetName());
		
		// Auto-find RVT Actor if no target texture is set
		if (!TargetVirtualTexture)
		{
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARSVirtualTextureActor::StaticClass(), FoundActors);
			if (FoundActors.Num() > 0)
			{
				if (ARSVirtualTextureActor* RVTActor = Cast<ARSVirtualTextureActor>(FoundActors[0]))
				{
					TargetVirtualTexture = RVTActor->VirtualTexture;
					UE_LOG(LogTemp, Log, TEXT("UAreaDecalComponent::BeginPlay - Auto-found RVT Actor: %s (Texture: %s)"), 
						*RVTActor->GetName(), 
						TargetVirtualTexture ? *TargetVirtualTexture->GetName() : TEXT("NULL"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UAreaDecalComponent::BeginPlay - No ARSVirtualTextureActor found in world!"));
			}
		}
	}

	UpdateDecalVisuals();
}

void UAreaDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 1. Visibility check (throttled to VisibilityCheckInterval)
	TimeSinceLastVisibilityCheck += DeltaTime;
	if (TimeSinceLastVisibilityCheck >= VisibilityCheckInterval)
	{
		TimeSinceLastVisibilityCheck = 0.f;

		if (!bDecalIsVisible) return;

		AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner());

		if (!OwningUnit)
		{
			UE_LOG(LogTemp, Error, TEXT("AreaDecalComponent's owner '%s' is not an AUnitBase! Cannot interact with Mass."), *GetOwner()->GetName());
			return;
		}

		bool bVisibility = OwningUnit->ComputeLocalVisibility();

		if (bUseRuntimeVirtualTexture)
		{
			// Keep the regular decal strictly disabled while RVT is active
			if (IsVisible())
			{
				SetVisibility(false);
				SetHiddenInGame(true);
			}

			// Only the RVT writer follows the unit visibility
			if (RVTWriterComponent)
			{
				RVTWriterComponent->SetVisibility(bVisibility);
				RVTWriterComponent->SetHiddenInGame(!bVisibility);
			}
		}
		else
		{
			// Regular decal path controls visibility; ensure RVT writer stays hidden
			if (IsVisible() != bVisibility)
			{
				SetVisibility(bVisibility);
				SetHiddenInGame(!bVisibility);
			}

			if (RVTWriterComponent)
			{
				RVTWriterComponent->SetVisibility(false);
				RVTWriterComponent->SetHiddenInGame(true);
			}
		}
	}
}



void UAreaDecalComponent::OnRep_CurrentMaterial()
{
	UpdateDecalVisuals();
}

void UAreaDecalComponent::OnRep_DecalColor()
{
	UpdateDecalVisuals();
}

void UAreaDecalComponent::OnRep_DecalRadius()
{
	// If we are currently scaling via multicast/timer, don't let OnRep snap the value back.
	// The timer will finish and set the final CurrentDecalRadius.
	if (!bIsScaling)
	{
		UpdateDecalVisuals();
	}
}

void UAreaDecalComponent::OnRep_UseRuntimeVirtualTexture()
{
	UpdateDecalVisuals();
}

void UAreaDecalComponent::UpdateDecalVisuals()
{
	// --- Normal Decal Logic ---
	if (!bUseRuntimeVirtualTexture)
	{
		// Hide RVT writer if it exists
		if (RVTWriterComponent)
		{
			RVTWriterComponent->SetHiddenInGame(true);
			RVTWriterComponent->SetVisibility(false);
		}

		// If the material is null, the decal should be hidden.
		if (!CurrentMaterial)
		{
			SetHiddenInGame(true);
			return;
		}

		SetHiddenInGame(false);

		// Create a dynamic material instance if we don't have one or if the base material has changed.
		if (!DynamicDecalMaterial || DynamicDecalMaterial->Parent != CurrentMaterial)
		{
			DynamicDecalMaterial = UMaterialInstanceDynamic::Create(CurrentMaterial, this);
			SetDecalMaterial(DynamicDecalMaterial);
		}

		if (DynamicDecalMaterial)
		{
			// Apply the replicated color. Your material needs a Vector Parameter named "Color".
			DynamicDecalMaterial->SetVectorParameterValue("Color", CurrentDecalColor);
		}
		
		// Apply the current radius.
		DecalSize = FVector(DecalSize.X, CurrentDecalRadius, CurrentDecalRadius);
		MarkRenderStateDirty();
	}
	else
	{
		// --- RVT Logic ---
		
		// Hide the normal decal
		SetHiddenInGame(true);
		SetVisibility(false);

		if (!TargetVirtualTexture || !RVTWriterMaterial)
		{
			if (RVTWriterComponent)
			{
				RVTWriterComponent->SetHiddenInGame(true);
				RVTWriterComponent->SetVisibility(false);
			}
			return;
		}

		if (RVTWriterComponent)
		{
			// Render Pass setup (should be exclusive to RVT)
			RVTWriterComponent->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;
			RVTWriterComponent->bRenderInMainPass = false;
			RVTWriterComponent->SetCastShadow(false);
			RVTWriterComponent->SetReceivesDecals(false);

			// Only show and write if radius is meaningful
			bool bShouldBeActive = (CurrentDecalRadius > 0.1f);
			RVTWriterComponent->SetHiddenInGame(!bShouldBeActive);
			RVTWriterComponent->SetVisibility(bShouldBeActive);

			if (bShouldBeActive)
			{
				UE_LOG(LogTemp, Log, TEXT("UpdateDecalVisuals (RVT) - Active for %s. VT: %s, Mat: %s, Radius: %f, Color: %s"), 
					*GetOwner()->GetName(),
					*TargetVirtualTexture->GetName(),
					*RVTWriterMaterial->GetName(),
					CurrentDecalRadius,
					*CurrentDecalColor.ToString());
			}

			// 1. Mesh Update
			if (RVTWriterCustomMesh)
			{
				if (RVTWriterComponent->GetStaticMesh() != RVTWriterCustomMesh)
				{
					RVTWriterComponent->SetStaticMesh(RVTWriterCustomMesh);
				}
			}

			// 2. Material Update (Dynamic Instance to pass Color)
			if (!RVTWriterDynamicMaterial || RVTWriterDynamicMaterial->Parent != RVTWriterMaterial)
			{
				RVTWriterDynamicMaterial = UMaterialInstanceDynamic::Create(RVTWriterMaterial, this);
				RVTWriterComponent->SetMaterial(0, RVTWriterDynamicMaterial);
			}

			if (RVTWriterDynamicMaterial)
			{
				RVTWriterDynamicMaterial->SetVectorParameterValue("Color", CurrentDecalColor);
			}
			
			// 3. RVT Assignment
			RVTWriterComponent->RuntimeVirtualTextures.Empty();
			RVTWriterComponent->RuntimeVirtualTextures.Add(TargetVirtualTexture);
			
			// 4. Scale Logic
			// Assuming base mesh size is 100 units (like the Engine Plane).
			// If a custom mesh is used, it might need different scaling, but 100 is a safe default for most marker meshes.
			float MeshBaseSize = 100.0f;
			float ScaleFactor = (CurrentDecalRadius * 2.0f) / MeshBaseSize;
			RVTWriterComponent->SetRelativeScale3D(FVector(ScaleFactor, ScaleFactor, 1.0f));
			
			// Slightly offset the writer to avoid being exactly at the same plane as the ground if needed
			// although with Exclusive pass this usually doesn't matter.
			RVTWriterComponent->SetRelativeLocation(FVector(0.f, 0.f, 2.0f));

			// Mark render state dirty to ensure RVT update
			RVTWriterComponent->MarkRenderStateDirty();
		}
	}
}



// ----------------------------------------------------------------------------------
// SERVER-SIDE FUNCTIONS
// ----------------------------------------------------------------------------------

void UAreaDecalComponent::Server_ActivateDecal_Implementation(UMaterialInterface* NewMaterial, FLinearColor NewColor, float NewRadius)
{
	if (!NewMaterial || NewRadius <= 0.f)
	{
		// If activation parameters are invalid, deactivate instead.
		Server_DeactivateDecal();
		return;
	}

	// If we are scaling currently, stop it
	if (bIsScaling)
	{
		bIsScaling = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ScaleTimerHandle);
		}
	}

	bDecalIsVisible = true;
	// Update the replicated properties. This will trigger the OnRep functions on all clients.
	CurrentMaterial = NewMaterial;
	CurrentDecalColor = NewColor;
	CurrentDecalRadius = NewRadius;

	// The server also needs to see the change immediately.
	UpdateDecalVisuals();

	// 1. Cast the owner to AUnitBase
	AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner());

	// 2. CRITICAL: Check if the cast was successful before using the pointer!
	if (!OwningUnit)
	{
		UE_LOG(LogTemp, Error, TEXT("AreaDecalComponent's owner '%s' is not an AUnitBase! Cannot interact with Mass."), *GetOwner()->GetName());
		return;
	}

	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	
	if (!OwningUnit->GetMassEntityData(EntityManager, EntityHandle))
	{
		// Error already logged in GetMassEntityData
		return;
	}

	// Check if entity is still valid
	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("ASpawnerUnit (%s): RemoveTagFromEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return;
	}
	
	FMassGameplayEffectFragment* EffectFragment = EntityManager->GetFragmentDataPtr<FMassGameplayEffectFragment>(EntityHandle);

	if (EffectFragment)
	{
		if (FriendlyEffect)
			EffectFragment->FriendlyEffect = FriendlyEffect;
		
		if (EnemyEffect)
			EffectFragment->EnemyEffect    = EnemyEffect;
		
		EffectFragment->EffectRadius   = NewRadius;
	}
}

void UAreaDecalComponent::Server_DeactivateDecal_Implementation()
{
	// Stop scaling if active
	if (bIsScaling)
	{
		bIsScaling = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ScaleTimerHandle);
		}
	}

	// The server hides it immediately.
	bDecalIsVisible = false;
	// Setting the material to null is the signal for deactivation. This will replicate.
	CurrentMaterial = nullptr;
	CurrentDecalRadius = 0.f;
	SetHiddenInGame(true, true);
	UpdateDecalVisuals();
	// OnRep_CurrentMaterial will be called on clients, which will then also hide it.
}

void UAreaDecalComponent::Server_ScaleDecalToRadius_Implementation(float EndRadius, float TimeSeconds, bool OwnerIsBeacon)
{
	Multicast_ScaleDecalToRadius(EndRadius, TimeSeconds, OwnerIsBeacon);
}

void UAreaDecalComponent::Multicast_ScaleDecalToRadius_Implementation(float EndRadius, float TimeSeconds, bool OwnerIsBeacon)
{
	if (EndRadius < 0.f)
	{
		EndRadius = 0.f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Cancel any previous scaling
	if (bIsScaling)
	{
		World->GetTimerManager().ClearTimer(ScaleTimerHandle);
		bIsScaling = false;
	}

	// Immediate set if time <= 0
	if (TimeSeconds <= KINDA_SMALL_NUMBER)
	{
		CurrentDecalRadius = EndRadius;
		if (GetNetMode() == NM_DedicatedServer)
		{
			UpdateMassEffectRadius(EndRadius);
		}
		UpdateDecalVisuals();

		// If owner is a beacon, propagate radius immediately
		if (OwnerIsBeacon)
		{
			if (ABuildingBase* Building = Cast<ABuildingBase>(GetOwner()))
			{
				Building->SetBeaconRange(EndRadius);
			}
		}
		return;
	}

	// Initialize scaling state
	ScaleStartRadius = CurrentDecalRadius;
	ScaleTargetRadius = EndRadius;
	ScaleDuration = TimeSeconds;
	ScaleStartTime = World->GetTimeSeconds();
	bIsScaling = true;
	bScaleOwnerIsBeacon = OwnerIsBeacon;

	// Ensure decal visible while scaling
	bDecalIsVisible = true;

	// Start timer
	const float Interval = FMath::Max(0.01f, ScaleUpdateInterval);
	World->GetTimerManager().SetTimer(ScaleTimerHandle, this, &UAreaDecalComponent::HandleScaleStep, Interval, true);
}

void UAreaDecalComponent::HandleScaleStep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bIsScaling)
	{
		World->GetTimerManager().ClearTimer(ScaleTimerHandle);
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Elapsed = Now - ScaleStartTime;
	float Alpha = (ScaleDuration > 0.f) ? (Elapsed / ScaleDuration) : 1.f;
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

	const float NewRadius = FMath::Lerp(ScaleStartRadius, ScaleTargetRadius, Alpha);
	CurrentDecalRadius = NewRadius;
	
	// Server-only updates
	if (GetNetMode() == NM_DedicatedServer)
	{
		UpdateMassEffectRadius(NewRadius);
	}
	
	UpdateDecalVisuals();

	// If the owner is a beacon, update its beacon range every tick with the current radius
	if (bScaleOwnerIsBeacon)
	{
		if (ABuildingBase* Building = Cast<ABuildingBase>(GetOwner()))
		{
			Building->SetBeaconRange(NewRadius);
		}
	}

	if (Alpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		// Finish
		bIsScaling = false;
		World->GetTimerManager().ClearTimer(ScaleTimerHandle);
	}
}

void UAreaDecalComponent::UpdateMassEffectRadius(float NewRadius)
{
	AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner());
	if (!OwningUnit)
	{
		return;
	}

	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	if (!OwningUnit->GetMassEntityData(EntityManager, EntityHandle))
	{
		return;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		return;
	}

	FMassGameplayEffectFragment* EffectFragment = EntityManager->GetFragmentDataPtr<FMassGameplayEffectFragment>(EntityHandle);
	if (EffectFragment)
	{
		EffectFragment->EffectRadius = NewRadius;
	}
}



