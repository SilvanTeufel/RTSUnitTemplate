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
#include "Actors/EffectArea.h"
#include "Mass/Abilitys/DecalScalingFragments.h"
#include "MassEntityManager.h"
#include "Actors/FogActor.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "Mass/MassActorBindingComponent.h"

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

	// Cache the default plane mesh for RVT writer (will be created dynamically at runtime)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		DefaultPlaneMesh = PlaneMeshAsset.Object;
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

	// Create RVTWriterComponent dynamically at runtime to avoid template mismatch during cook.
	// CreateDefaultSubobject inside a component constructor causes nested subobject issues when
	// the component is Blueprint-added (template vs instance hierarchy confusion).
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		RVTWriterComponent = NewObject<UStaticMeshComponent>(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("RVTWriterMesh"), RF_Transient);
		if (RVTWriterComponent)
		{
			RVTWriterComponent->SetCastShadow(false);
			RVTWriterComponent->SetReceivesDecals(false);
			RVTWriterComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			RVTWriterComponent->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;
			RVTWriterComponent->bRenderInMainPass = false;
			RVTWriterComponent->SetHiddenInGame(true);
			RVTWriterComponent->SetVisibility(false);

			// Set default plane mesh
			if (DefaultPlaneMesh)
			{
				RVTWriterComponent->SetStaticMesh(DefaultPlaneMesh);
			}

			// Scale based on RVTWriterMeshSize
			if (RVTWriterMeshSize <= 0.f)
			{
				RVTWriterMeshSize = 2000.f;
			}
			const float InitialScale = RVTWriterMeshSize / 100.f;
			RVTWriterComponent->SetRelativeScale3D(FVector(InitialScale, InitialScale, 1.0f));

			// Attach to the decal's parent (or actor root) to avoid inheriting the decal's 90deg rotation
			USceneComponent* AttachTarget = GetAttachParent();
			if (!AttachTarget && GetOwner())
			{
				AttachTarget = GetOwner()->GetRootComponent();
			}

			if (AttachTarget)
			{
				RVTWriterComponent->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform);
				// Match decal's relative location but keep flat orientation
				FVector TargetLocation = GetRelativeLocation();
				TargetLocation.Z += 2.0f;
				RVTWriterComponent->SetRelativeLocation(TargetLocation);
				RVTWriterComponent->SetRelativeRotation(FRotator::ZeroRotator);
			}

			RVTWriterComponent->RegisterComponent();
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
					// Read the RVT off the actor's component, not off the actor: the actor's own
					// VirtualTexture property was always unset on placed instances, so this
					// auto-find used to resolve to null and RVT mode silently did nothing.
					TargetVirtualTexture = RVTActor->GetVirtualTexture();
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

	if (bRevealToEnemiesByFog && GetNetMode() != NM_DedicatedServer && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FogRevealTimer, this, &UAreaDecalComponent::UpdateFogReveal,
				FMath::Max(0.05f, FogRevealInterval), true, FMath::FRandRange(0.f, FogRevealInterval));
		}
	}
}

void UAreaDecalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FogRevealTimer);
	}
	Super::EndPlay(EndPlayReason);
}

int32 UAreaDecalComponent::GetOwnerTeamId() const
{
	if (const AUnitBase* Unit = Cast<AUnitBase>(GetOwner()))
	{
		return Unit->TeamId;
	}
	if (const AEffectArea* Area = Cast<AEffectArea>(GetOwner()))
	{
		return Area->TeamId;
	}
	return INDEX_NONE;
}

AFogActor* UAreaDecalComponent::GetMaskingFog()
{
	if (!bRevealToEnemiesByFog || !bUseRuntimeVirtualTexture || GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	const int32 OwnerTeam = GetOwnerTeamId();
	if (OwnerTeam == INDEX_NONE)
	{
		return nullptr;
	}
	return AFogActor::FindMaskingFogFor(GetWorld(), OwnerTeam, CachedFogActor);
}

FIntPoint UAreaDecalComponent::FogCell(const FVector2D& P) const
{
	const float Size = FMath::Max(25.f, FogRevealCellSize);
	return FIntPoint(FMath::FloorToInt(P.X / Size), FMath::FloorToInt(P.Y / Size));
}

void UAreaDecalComponent::UpdateFogReveal()
{
	if (!RVTWriterComponent || !bUseRuntimeVirtualTexture)
	{
		return;
	}
	AFogActor* Fog = GetMaskingFog();
	bool bChanged = false;
	const float Radius = CurrentDecalRadius;
	if (!bDecalIsVisible || Radius <= 0.1f)
	{
		// Gone completely: forgotten - a new area here has to be seen again.
		if (FogRevealedCells.Num() > 0)
		{
			FogRevealedCells.Reset();
			bChanged = true;
		}
	}
	else if (Fog)
	{
		// Only cells inside the area now: what grows later into the fog stays hidden until seen.
		const FVector Centre = RVTWriterComponent->GetComponentLocation();
		const float Cell = FMath::Max(25.f, FogRevealCellSize);
		const int32 MinX = FMath::FloorToInt((Centre.X - Radius) / Cell);
		const int32 MaxX = FMath::FloorToInt((Centre.X + Radius) / Cell);
		const int32 MinY = FMath::FloorToInt((Centre.Y - Radius) / Cell);
		const int32 MaxY = FMath::FloorToInt((Centre.Y + Radius) / Cell);
		const float RadiusSq = FMath::Square(Radius + Cell * 0.5f);
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				const FIntPoint Key(X, Y);
				if (FogRevealedCells.Contains(Key))
				{
					continue;
				}
				const FVector P((X + 0.5f) * Cell, (Y + 0.5f) * Cell, Centre.Z);
				if (FVector::DistSquared2D(P, Centre) <= RadiusSq && Fog->IsWorldPositionRevealed(P))
				{
					FogRevealedCells.Add(Key);
					bChanged = true;
				}
			}
		}
	}

	const bool bMasked = Fog != nullptr;
	// The writer moved (a building lifts off / lands): the mask has to follow it.
	const bool bMoved = bMasked && FogMaskTexels > 0
		&& FVector2D::Distance(FVector2D(RVTWriterComponent->GetComponentLocation()), FogMaskOrigin) > FogRevealCellSize * 0.5f;
	if (bChanged || bMoved || bMasked != bFogMasked)
	{
		bFogMasked = bMasked;
		if (bMasked)
		{
			RebuildFogMaskTexture();
		}
		UpdateDecalVisuals();
		if (RVTWriterComponent->IsVisible())
		{
			RVTWriterComponent->MarkRenderStateDirty();   // the virtual texture only redraws on a change
		}
	}
}

void UAreaDecalComponent::RebuildFogMaskTexture()
{
	if (!RVTWriterComponent)
	{
		return;
	}
	const float Size = RVTWriterComponent->GetComponentScale().X * 100.f;   // 100 uu base plane
	const float Cell = FMath::Max(25.f, FogRevealCellSize);
	const int32 Texels = FMath::Clamp(FMath::CeilToInt(Size / Cell), 4, 256);
	if (!FogMaskTexture || FogMaskTexels != Texels)
	{
		FogMaskTexture = UTexture2D::CreateTransient(Texels, Texels, PF_B8G8R8A8);
		if (!FogMaskTexture)
		{
			return;
		}
		// sRGB like the material's Color sampler; the mask only holds 0 and 255, which sRGB keeps exact.
		FogMaskTexture->SRGB = true;
		FogMaskTexture->Filter = TF_Bilinear;
		FogMaskTexture->AddressX = TA_Clamp;
		FogMaskTexture->AddressY = TA_Clamp;
		FogMaskTexture->UpdateResource();
		FogMaskTexels = Texels;
	}
	FogMaskOrigin = FVector2D(RVTWriterComponent->GetComponentLocation());
	FogMaskSize = Size;

	// Texel (x, y) covers world X / Y from the plane's corner; the material maps the same way (FogMaskOrigin,
	// FogMaskSize), so the texture does not depend on the plane's UVs.
	const int32 Count = Texels * Texels;
	FColor* Pixels = new FColor[Count];
	for (int32 Y = 0; Y < Texels; ++Y)
	{
		for (int32 X = 0; X < Texels; ++X)
		{
			const FVector2D P = FogMaskOrigin + FVector2D((X + 0.5f) / Texels - 0.5f, (Y + 0.5f) / Texels - 0.5f) * Size;
			const uint8 Value = FogRevealedCells.Contains(FogCell(P)) ? 255 : 0;
			Pixels[Y * Texels + X] = FColor(Value, Value, Value, 255);
		}
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Texels, Texels);
	FogMaskTexture->UpdateTextureRegions(0, 1, Region, Texels * sizeof(FColor), sizeof(FColor),
		reinterpret_cast<uint8*>(Pixels),
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] reinterpret_cast<FColor*>(SrcData);
			delete Regions;
		});
}

void UAreaDecalComponent::ApplyFogMaskParameters()
{
	if (!RVTWriterDynamicMaterial)
	{
		return;
	}
	const bool bUseMask = bFogMasked && FogMaskTexture && FogMaskSize > 1.f;
	RVTWriterDynamicMaterial->SetScalarParameterValue(TEXT("FogMaskStrength"), bUseMask ? 1.f : 0.f);
	if (bUseMask)
	{
		RVTWriterDynamicMaterial->SetTextureParameterValue(TEXT("FogMask"), FogMaskTexture);
		RVTWriterDynamicMaterial->SetVectorParameterValue(TEXT("FogMaskOrigin"), FLinearColor(FogMaskOrigin.X, FogMaskOrigin.Y, 0.f, 0.f));
		RVTWriterDynamicMaterial->SetScalarParameterValue(TEXT("FogMaskSize"), FogMaskSize);
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
	if (HasAnyFlags(RF_ClassDefaultObject)) return;
	
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

			// Only show and write if radius is meaningful - and for an enemy under fog of war once some of it
			// was seen.
			bool bShouldBeActive = (CurrentDecalRadius > 0.1f) && (!bFogMasked || FogRevealedCells.Num() > 0);
			RVTWriterComponent->SetHiddenInGame(!bShouldBeActive);
			RVTWriterComponent->SetVisibility(bShouldBeActive);


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
				ApplyFogMaskParameters();
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
			if (RVTWriterComponent->GetAttachParent() != this)
			{
				TargetLocation = GetRelativeLocation();
				TargetLocation.Z += 2.0f;
			}

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

	// 1. Get Mass data from owner
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSubsystem) return;
	FMassEntityManager& EM = MassSubsystem->GetMutableEntityManager();
	
	FMassEntityManager* EntityManager = &EM;
	FMassEntityHandle EntityHandle;
	bool bHasMassData = false;

	if (AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner()))
	{
		bHasMassData = OwningUnit->GetMassEntityData(EntityManager, EntityHandle);
	}
	else if (UMassActorBindingComponent* BindingComp = GetOwner()->FindComponentByClass<UMassActorBindingComponent>())
	{
		EntityHandle = BindingComp->GetMassEntityHandle();
		bHasMassData = EntityHandle.IsValid();
	}

	if (!bHasMassData)
	{
		return;
	}

	// Check if entity is still valid
	if (!EntityManager->IsEntityValid(EntityHandle))
	{
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
		EffectFragment->bAffectFlyingUnits = bAffectFlyingUnits;
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
			else if (AEffectArea* EffectArea = Cast<AEffectArea>(GetOwner()))
			{
				EffectArea->SetBeaconRange(EndRadius);
			}
		}
		return;
	}

	// Switch to Mass-based scaling: initialize component state and add the processing tag on both server and clients
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSubsystem) return;
	FMassEntityManager& EM = MassSubsystem->GetMutableEntityManager();

	FMassEntityManager* EntityManager = &EM;
	FMassEntityHandle EntityHandle;
	bool bHasMassData = false;

	if (AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner()))
	{
		bHasMassData = OwningUnit->GetMassEntityData(EntityManager, EntityHandle);
	}
	else if (UMassActorBindingComponent* BindingComp = GetOwner()->FindComponentByClass<UMassActorBindingComponent>())
	{
		EntityHandle = BindingComp->GetMassEntityHandle();
		bHasMassData = EntityHandle.IsValid();
	}

	if (!bHasMassData)
	{
		return;
	}

	// Initialize local scaling state on the component
	StartMassScaling(CurrentDecalRadius, EndRadius, TimeSeconds, OwnerIsBeacon);

	if (EntityManager && EntityManager->IsEntityValid(EntityHandle))
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

void UAreaDecalComponent::ApplyReplayState(float NewRadius)
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	bDecalIsVisible = true;
	CurrentDecalRadius = NewRadius;
	UpdateDecalVisuals();
}

void UAreaDecalComponent::SetCurrentDecalRadiusFromMass(float NewRadius)
{
	CurrentDecalRadius = NewRadius;
	UpdateDecalVisuals();
}

void UAreaDecalComponent::UpdateMassEffectRadius(float NewRadius)
{
	// 1. Get Mass data from owner
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSubsystem) return;
	FMassEntityManager& EM = MassSubsystem->GetMutableEntityManager();

	FMassEntityManager* EntityManager = &EM;
	FMassEntityHandle EntityHandle;
	bool bHasMassData = false;

	if (AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner()))
	{
		bHasMassData = OwningUnit->GetMassEntityData(EntityManager, EntityHandle);
	}
	else if (UMassActorBindingComponent* BindingComp = GetOwner()->FindComponentByClass<UMassActorBindingComponent>())
	{
		EntityHandle = BindingComp->GetMassEntityHandle();
		bHasMassData = EntityHandle.IsValid();
	}

	if (!bHasMassData)
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




void UAreaDecalComponent::SetVirtualTextureWritingEnabled(bool bEnabled)
{
	if (!RVTWriterComponent)
	{
		return;
	}

	if (!bEnabled)
	{
		// Zustand merken, BEVOR er ueberschrieben wird - sonst weckt das Wiedereinschalten einen
		// Writer auf, der vorher gar nicht aktiv war.
		bVirtualTextureWritingWasEnabled = RVTWriterComponent->IsVisible();
		RVTWriterComponent->SetHiddenInGame(true);
		RVTWriterComponent->SetVisibility(false);
		return;
	}

	if (bVirtualTextureWritingWasEnabled)
	{
		RVTWriterComponent->SetHiddenInGame(false);
		RVTWriterComponent->SetVisibility(true);
	}
}
