// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AreaDecalComponent.h" // Adjust path if necessary

#include "Characters/Unit/UnitBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Characters/Unit/BuildingBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "VT/RuntimeVirtualTexture.h"
#include "UObject/ConstructorHelpers.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/RSVirtualTextureActor.h"
#include "Mass/Abilitys/DecalScalingFragments.h"
#include "MassEntityManager.h"

UAreaDecalComponent::UAreaDecalComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.TickInterval = 0.f; 
	SetIsReplicatedByDefault(true);

	// Initial state is inactive.
	SetVisibility(false);
	SetHiddenInGame(true);
	CurrentMaterial = nullptr;
	CurrentDecalRadius = 0.f;
	StandardDecalRadiusDivider = 3.f;
	bDecalIsVisible = false;
	
	// Rotate the decal to project downwards onto the ground.
	SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	// Default size, will be overridden by activation.
	DecalSize = FVector(500.f, 100.f, 100.f); 

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
	// Anwenderdefinierte Mesh-Kantenlänge (in UU) -> Editor-Property 'RVTWriterMeshSize'
	if (RVTWriterMeshSize <= 0.f)
	{
		RVTWriterMeshSize = 2000.f; // Fallback-Default
	}
	const float InitialScale = RVTWriterMeshSize / 100.f; // Engine-Plane ist 100 UU breit
	RVTWriterComponent->SetRelativeScale3D(FVector(InitialScale, InitialScale, 1.0f));

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
		//UE_LOG(LogTemp, Log, TEXT("UAreaDecalComponent::BeginPlay - RVT mode active for '%s'"), *GetOwner()->GetName());
		
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
					/*UE_LOG(LogTemp, Log, TEXT("UAreaDecalComponent::BeginPlay - Auto-found RVT Actor: %s (Texture: %s)"), 
						*RVTActor->GetName(), 
						TargetVirtualTexture ? *TargetVirtualTexture->GetName() : TEXT("NULL"));*/
				}
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("UAreaDecalComponent::BeginPlay - No ARSVirtualTextureActor found in world!"));
			}
		}
	}

	UpdateDecalVisuals();
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
	// Always update visuals when radius changes, even if scaling.
	// This ensures that clients see the server's replicated state if their local scaling isn't running.
	UpdateDecalVisuals();
}

void UAreaDecalComponent::OnRep_UseRuntimeVirtualTexture()
{
	UpdateDecalVisuals();
}

void UAreaDecalComponent::OnRep_DecalIsVisible()
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
		if (!CurrentMaterial || !bDecalIsVisible)
		{
			SetHiddenInGame(true);
			SetVisibility(false);
			return;
		}

		SetHiddenInGame(false);
		SetVisibility(true);

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
		// Note: DecalSize represents the full size of the decal box. 
		// If CurrentDecalRadius is the radius, we need to set the box width and height to 2 * Radius (the diameter).
		const float VisualRadius = StandardDecalRadiusDivider > 0.01f ? (CurrentDecalRadius / StandardDecalRadiusDivider) : CurrentDecalRadius;
		const float Depth = DecalSize.X > 0.1f ? DecalSize.X : 500.f;
		DecalSize = FVector(Depth, VisualRadius * 2.f, VisualRadius * 2.f);
		MarkRenderStateDirty();
	}
	else
	{
		// --- RVT Logic ---
		
		// Hide the normal decal
		SetHiddenInGame(true);
		SetVisibility(false);

		if (!TargetVirtualTexture || !RVTWriterMaterial || !bDecalIsVisible)
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
			if (RVTWriterComponent->VirtualTextureRenderPassType != ERuntimeVirtualTextureMainPassType::Exclusive)
			{
				RVTWriterComponent->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;
			}
			
			if (RVTWriterComponent->bRenderInMainPass)
			{
				RVTWriterComponent->bRenderInMainPass = false;
			}
			
			if (RVTWriterComponent->CastShadow)
			{
				RVTWriterComponent->SetCastShadow(false);
			}
			
			if (RVTWriterComponent->bReceivesDecals)
			{
				RVTWriterComponent->SetReceivesDecals(false);
			}

			// Only show and write if radius is meaningful
			bool bShouldBeActive = (CurrentDecalRadius > 0.1f);
			if (RVTWriterComponent->GetVisibleFlag() != bShouldBeActive)
			{
				RVTWriterComponent->SetHiddenInGame(!bShouldBeActive);
				RVTWriterComponent->SetVisibility(bShouldBeActive);
			}


			// 1. Mesh Update
			if (RVTWriterCustomMesh)
			{
				if (RVTWriterComponent->GetStaticMesh() != RVTWriterCustomMesh)
				{
					RVTWriterComponent->SetStaticMesh(RVTWriterCustomMesh);
				}
			}

			// 2. Mesh Scale Update aus Editor-Property
			if (RVTWriterMeshSize <= 0.f)
			{
				RVTWriterMeshSize = 2000.f;
			}
			const float TargetScale = RVTWriterMeshSize / 100.0f;
			if (FMath::Abs(RVTWriterComponent->GetRelativeScale3D().X - TargetScale) > 0.01f)
			{
				RVTWriterComponent->SetRelativeScale3D(FVector(TargetScale, TargetScale, 1.0f));
				RVTWriterComponent->MarkRenderStateDirty();
			}

			// 3. Material Update (Dynamic Instance to pass Color)
			if (!RVTWriterDynamicMaterial || RVTWriterDynamicMaterial->Parent != RVTWriterMaterial)
			{
				RVTWriterDynamicMaterial = UMaterialInstanceDynamic::Create(RVTWriterMaterial, this);
				RVTWriterComponent->SetMaterial(0, RVTWriterDynamicMaterial);
			}

			if (RVTWriterDynamicMaterial)
			{
				RVTWriterDynamicMaterial->SetVectorParameterValue("Color", CurrentDecalColor);
				
				// Normalisierung des Radius relativ zur Mesh-Halbkante (0..0.5)
				const float MeshWidth = RVTWriterComponent->GetRelativeScale3D().X * 100.0f; // 100 UU Basismesh
				const float RawNormalizedRadius = (MeshWidth > 0.1f) ? (CurrentDecalRadius / MeshWidth) : 0.0f; // 0..0.5 idealerweise
				const float Alpha = FMath::Clamp(RawNormalizedRadius / 0.5f, 0.f, 1.f);
				// Bereich auf [RVTMaterialRadiusMin..RVTMaterialRadiusMax] abbilden
				const float RangeMin = RVTMaterialRadiusMin;
				const float RangeMax = RVTMaterialRadiusMax > RVTMaterialRadiusMin ? RVTMaterialRadiusMax : RVTMaterialRadiusMin; // Schutz
				float FinalRadiusParam = FMath::Lerp(RangeMin, RangeMax, Alpha);

				// Optional: quantize the value passed to the material to N decimal places (default 2)
				int32 Precision = FMath::Clamp(RVTMaterialRadiusPrecision, 0, 4);
				if (Precision > 0)
				{
					const float Step = FMath::Pow(10.0f, -Precision);
					FinalRadiusParam = FMath::RoundToFloat(FinalRadiusParam / Step) * Step;
				}
				RVTWriterDynamicMaterial->SetScalarParameterValue("Radius", FinalRadiusParam);

			}
			
			// 3. RVT Assignment (only update if it changed)
			if (RVTWriterComponent->RuntimeVirtualTextures.Num() != 1 || RVTWriterComponent->RuntimeVirtualTextures[0] != TargetVirtualTexture)
			{
				RVTWriterComponent->RuntimeVirtualTextures.Empty();
				RVTWriterComponent->RuntimeVirtualTextures.Add(TargetVirtualTexture);
			}
			
			// 4. Scale Logic - Disabled to avoid RVT flickering.
			// The mesh size should be kept constant. Radius is passed as a material parameter "Radius".
			
			// Position update (only if necessary)
			FVector TargetLocation = FVector(0.f, 0.f, 2.0f);
			if (!RVTWriterComponent->GetRelativeLocation().Equals(TargetLocation, 0.1f))
			{
				RVTWriterComponent->SetRelativeLocation(TargetLocation);
			}

			// Mark render state dirty only if active and radius or color changed significantly to avoid flickering
			bool bColorChanged = !CurrentDecalColor.Equals(LastNotifiedColor, 0.01f);
			bool bRadiusChanged = FMath::Abs(CurrentDecalRadius - LastNotifiedRadius) > 0.5f;

			if (bShouldBeActive && (bColorChanged || bRadiusChanged))
			{
				RVTWriterComponent->MarkRenderStateDirty();
				LastNotifiedRadius = CurrentDecalRadius;
				LastNotifiedColor = CurrentDecalColor;
			}
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

	bIsScaling = false;
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
	bIsScaling = false;

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

	bIsScaling = false;

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

	// Switch to Mass-based scaling: initialize component state and add the processing tag on both server and clients
	AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner());
	if (!OwningUnit)
	{
		return;
	}

	// Initialize local scaling state on the component
	StartMassScaling(CurrentDecalRadius, EndRadius, TimeSeconds, OwnerIsBeacon);

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (OwningUnit->GetMassEntityData(EntityManager, EntityHandle) && EntityManager && EntityManager->IsEntityValid(EntityHandle))
	{
		EntityManager->Defer().AddTag<FMassDecalScalingTag>(EntityHandle);
	}

	// Make sure decal visuals are enabled while scaling; processor will drive the actual radius
	bDecalIsVisible = true;
	UpdateDecalVisuals();
}

void UAreaDecalComponent::StartMassScaling(float InStartRadius, float InTargetRadius, float InDurationSeconds, bool bInOwnerIsBeacon)
{
	ScaleStartRadius = InStartRadius;
	ScaleTargetRadius = InTargetRadius;
	ScaleDuration = InDurationSeconds;
	MassElapsedTime = 0.f;
	bScaleOwnerIsBeacon = bInOwnerIsBeacon;
	bIsScaling = true;
	bDecalIsVisible = true;
	UpdateDecalVisuals();
}

bool UAreaDecalComponent::AdvanceMassScaling(float DeltaSeconds, float& OutNewRadius, bool& bOutCompleted)
{
	if (ScaleDuration <= KINDA_SMALL_NUMBER)
	{
		OutNewRadius = ScaleTargetRadius;
		bOutCompleted = true;
		bIsScaling = false;
		return true;
	}

	MassElapsedTime += DeltaSeconds;
	float Alpha = FMath::Clamp(MassElapsedTime / ScaleDuration, 0.f, 1.f);
	OutNewRadius = FMath::Lerp(ScaleStartRadius, ScaleTargetRadius, Alpha);
	bOutCompleted = (Alpha >= 1.f - KINDA_SMALL_NUMBER);
	
	if (bOutCompleted)
	{
		bIsScaling = false;
	}
	
	return true;
}

void UAreaDecalComponent::SetCurrentDecalRadiusFromMass(float NewRadius)
{
	CurrentDecalRadius = NewRadius;
	UpdateDecalVisuals();
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



