// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Unit/ConstructionUnit.h"
#include "Net/UnrealNetwork.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CollisionUtils.h"
#include "GameFramework/Actor.h"
#include "Core/UnitData.h"
#include "Mass/MassActorBindingComponent.h"
#include "Mass/MassUnitVisualFragments.h"

AConstructionUnit::AConstructionUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Name = TEXT("ConstructionSite");
	// Likely stationary
	CanMove = false;
	bIsConstructionUnit = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AConstructionUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AConstructionUnit, Worker);
	DOREPLIFETIME(AConstructionUnit, WorkArea);
	DOREPLIFETIME(AConstructionUnit, bIndicatorFootprintUseBox);
	DOREPLIFETIME(AConstructionUnit, IndicatorFootprintBoxExtent);
	DOREPLIFETIME(AConstructionUnit, IndicatorFootprintCapsuleRadius);
	DOREPLIFETIME(AConstructionUnit, Rep_VE_OscillationOffsetA);
	DOREPLIFETIME(AConstructionUnit, Rep_VE_OscillationOffsetB);
	DOREPLIFETIME(AConstructionUnit, Rep_VE_OscillationCyclesPerSecond);
	DOREPLIFETIME(AConstructionUnit, DroneBehavior);
}

void AConstructionUnit::BeginPlay()
{
	Super::BeginPlay();
	OpenHealthWidget = true;
	bShowLevelOnly = false;
}

void AConstructionUnit::SetCharacterVisibility(bool desiredVisibility)
{
	Super::SetCharacterVisibility(desiredVisibility);
	if (WorkArea != nullptr && WorkArea->Mesh != nullptr)
	{
		WorkArea->Mesh->SetHiddenInGame(!desiredVisibility);
	}
}

void AConstructionUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

namespace
{
	// AABB half-extents of a yaw-rotated rectangle: exact for cardinal (90-degree) deltas,
	// mildly conservative in between.
	FVector2D RotatedAABBExtents2D(const FVector2D& Extents, float YawDeg)
	{
		const float Rad = FMath::DegreesToRadians(YawDeg);
		const float C = FMath::Abs(FMath::Cos(Rad));
		const float S = FMath::Abs(FMath::Sin(Rad));
		return FVector2D(C * Extents.X + S * Extents.Y, S * Extents.X + C * Extents.Y);
	}
}

void AConstructionUnit::SeedIndicatorFootprint(const AWorkArea* InWorkArea)
{
	bIndicatorFootprintUseBox = false;
	IndicatorFootprintBoxExtent = FVector::ZeroVector;
	IndicatorFootprintCapsuleRadius = 0.f;

	if (!InWorkArea)
	{
		return;
	}

	// The HUD rotates the indicator by THIS unit's actor rotation, so all extents are stored
	// in this site's local frame. The site may spawn at a different yaw than the finished
	// building (worker path spawns at yaw 0, the building at ServerMeshRotationBuilding).
	const float OwnYaw = GetActorRotation().Yaw;

	if (InWorkArea->BuildingClass)
	{
		// Same box the finished building's Mass binding captures for the HUD
		// (InitializeMassEntityStatsFromOwner uses GetUnscaledBoxExtent on the tagged box),
		// rotated by the building-vs-site yaw delta so the drawn world rect matches.
		if (const UBoxComponent* BuildingBox = FCollisionUtils::FindTaggedBoxTemplateOnClass(InWorkArea->BuildingClass))
		{
			const FVector Extent = BuildingBox->GetUnscaledBoxExtent();
			const FVector2D LocalXY = RotatedAABBExtents2D(
				FVector2D(Extent.X, Extent.Y),
				InWorkArea->ServerMeshRotationBuilding.Yaw - OwnYaw);
			bIndicatorFootprintUseBox = true;
			IndicatorFootprintBoxExtent = FVector(LocalXY.X, LocalXY.Y, Extent.Z);
			return;
		}

		// No tagged box: mirror the building's capsule capture (scaled radius + the binding
		// component's AdditionalCapsuleRadius).
		if (const ABuildingBase* BuildingCDO = InWorkArea->BuildingClass->GetDefaultObject<ABuildingBase>())
		{
			float Radius = 0.f;
			if (const UCapsuleComponent* Capsule = BuildingCDO->GetCapsuleComponent())
			{
				Radius = Capsule->GetScaledCapsuleRadius();
			}
			if (const UMassActorBindingComponent* Binding = BuildingCDO->FindComponentByClass<UMassActorBindingComponent>())
			{
				Radius += Binding->AdditionalCapsuleRadius;
			}
			if (Radius > KINDA_SMALL_NUMBER)
			{
				IndicatorFootprintCapsuleRadius = Radius;
				return;
			}
		}
	}

	// Fallback: the WorkArea mesh's world footprint is by definition the building's ground
	// plot. The world AABB is axis-aligned, so un-rotate it into this site's local frame.
	if (InWorkArea->Mesh)
	{
		const FVector AreaExtent = InWorkArea->Mesh->Bounds.GetBox().GetExtent();
		if (AreaExtent.X > KINDA_SMALL_NUMBER && AreaExtent.Y > KINDA_SMALL_NUMBER)
		{
			const FVector2D LocalXY = RotatedAABBExtents2D(FVector2D(AreaExtent.X, AreaExtent.Y), -OwnYaw);
			bIndicatorFootprintUseBox = true;
			IndicatorFootprintBoxExtent = FVector(LocalXY.X, LocalXY.Y, AreaExtent.Z);
		}
	}
}

FBox AConstructionUnit::ComputeVisualBounds(const AUnitBase* Unit)
{
	FBox VisualBox(ForceInit);
	if (!Unit)
	{
		return VisualBox;
	}

	const bool bSkeletal = Unit->bUseSkeletalMovement;

	Unit->ForEachComponent<UMeshComponent>(false, [&VisualBox, bSkeletal](const UMeshComponent* MeshComp)
	{
		if (!IsValid(MeshComp) || !MeshComp->IsRegistered())
		{
			return;
		}

		// Include-only filter: anything not matched below is skipped — in particular
		// UWidgetComponent, which DERIVES from UMeshComponent and must not skew the box.
		// Order matters: ISM derives from UStaticMeshComponent.
		if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(MeshComp))
		{
			if (!ISM->GetStaticMesh() || ISM->GetInstanceCount() == 0)
			{
				return;
			}
		}
		else if (const UStaticMeshComponent* StaticComp = Cast<UStaticMeshComponent>(MeshComp))
		{
			if (!StaticComp->GetStaticMesh())
			{
				return;
			}
		}
		else if (const USkeletalMeshComponent* SkeletalComp = Cast<USkeletalMeshComponent>(MeshComp))
		{
			// ISM-mode units keep a hidden skeletal mesh at the capsule — leave it out.
			if (!bSkeletal || !SkeletalComp->GetSkeletalMeshAsset())
			{
				return;
			}
		}
		else
		{
			// Widgets and any other UMeshComponent subclass.
			return;
		}

		// CalcBounds instead of the cached Bounds: the ISM instance added during
		// FinishSpawning (InitializeUnitMode) only invalidates the bounds cache, so right
		// after spawn the cached value is still the pre-instance zero-extent point.
		VisualBox += MeshComp->CalcBounds(MeshComp->GetComponentTransform()).GetBox();
	});

	return VisualBox;
}

void AConstructionUnit::AlignConstructionToArea(AUnitBase* NewConstruction, const FVector& AnchorXY, float GroundZ)
{
	if (!NewConstruction)
	{
		return;
	}

	// Drone construction sites are exempt: they keep ActorScale = 1 while their capsule is
	// resized to the WorkArea, and their visuals (drone body, DronePlane, authored ISM
	// instances) sit at authored offsets driven later by the Mass drone simulation. Bounds-
	// centering on those offsets pushes the PIVOT — and the HUD selection ring, which draws
	// at the pivot — off the WorkArea (worst on small areas, where the area-sized capsule no
	// longer swallows the constant-size drone parts). The spawn location already IS the WA
	// pivot, which is exactly where the site, its ring, and the finished building belong.
	if (const AConstructionUnit* Site = Cast<AConstructionUnit>(NewConstruction))
	{
		if (Site->DroneBehavior)
		{
			return;
		}
	}

	FBox VisualBox = ComputeVisualBounds(NewConstruction);
	if (!VisualBox.IsValid)
	{
		// No mesh bounds (e.g. plain capsule-only unit): keep the old whole-actor behavior.
		VisualBox = NewConstruction->GetComponentsBoundingBox(true);
	}

	const FVector VisualCenter = VisualBox.GetCenter();
	FVector FinalLoc = NewConstruction->GetActorLocation();
	FinalLoc.X += AnchorXY.X - VisualCenter.X;
	FinalLoc.Y += AnchorXY.Y - VisualCenter.Y;
	if (!NewConstruction->IsFlying)
	{
		FinalLoc.Z += GroundZ - VisualBox.Min.Z;
	}
	NewConstruction->SetActorLocation(FinalLoc);
	NewConstruction->SyncTranslation();
}

void AConstructionUnit::SetDronePlaneISM(UInstancedStaticMeshComponent* InISM)
{
	DronePlaneISM = InISM;

	if (InISM)
	{
		if (!InISM->GetStaticMesh())
		{
			// The plane is rendered through the pooled Mass visual path, which silently drops a template
			// that has no static mesh — warn so a missing mesh doesn't read as "wiring failed".
			UE_LOG(LogTemp, Warning,
				TEXT("SetDronePlaneISM: DronePlane ISM on %s has no StaticMesh; the plane will not render via Mass."),
				*GetName());
		}

		// Register the plane as a Mass visual instance now. This works whether called before or after the
		// one-time Mass registration pass (which early-outs once registered), so it is safe from BeginPlay
		// and from the OnMassRegistrationFinished Blueprint event.
		RegisterAdditionalVisualNow(InISM);
	}

	// Propagate immediately if the visual fragment already exists (covers the drone-already-active case).
	// UUnitActorToFragmentSyncProcessor::SyncVisualEffect also refreshes this every frame.
	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->DronePlaneTemplateISM = InISM;
	}
}

UPrimitiveComponent* AConstructionUnit::ResolveVisualComponent() const
{
	if (VisualComponentOverride)
	{
		return VisualComponentOverride;
	}
	// Prefer a StaticMeshComponent if present
	TArray<UStaticMeshComponent*> Meshes;
	const_cast<AConstructionUnit*>(this)->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* C : Meshes)
	{
		if (IsValid(C)) return C;
	}
	// Fallback to root as primitive if possible
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		return Prim;
	}
	return nullptr;
}

void AConstructionUnit::MulticastStartRotateVisual_Implementation(const FVector& Axis, float DegreesPerSecond, float Duration)
{
	if (HasAuthority())
	{
		Rep_VE_ActiveEffects |= (1 << 1);
		Rep_VE_RotationAxis = Axis.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Axis.GetSafeNormal();
		Rep_VE_RotationDegreesPerSecond = DegreesPerSecond;
	}

	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->bRotationEnabled = true;
		EffectFrag->RotationAxis = Axis.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Axis.GetSafeNormal();
		EffectFrag->RotationDegreesPerSecond = DegreesPerSecond;
		EffectFrag->RotationDuration = Duration;
		EffectFrag->RotationElapsed = 0.f;
		UInstancedStaticMeshComponent* ResolvedISM = Cast<UInstancedStaticMeshComponent>(ResolveVisualComponent());
		EffectFrag->RotationTargetISM = ResolvedISM ? ResolvedISM : ISMComponent;
	}
}

void AConstructionUnit::MulticastStartOscillateVisual_Implementation(const FVector& LocalOffsetA, const FVector& LocalOffsetB, float CyclesPerSecond, float Duration)
{
	if (HasAuthority())
	{
		Rep_VE_ActiveEffects |= (1 << 2);
		Rep_VE_OscillationOffsetA = LocalOffsetA;
		Rep_VE_OscillationOffsetB = LocalOffsetB;
		Rep_VE_OscillationCyclesPerSecond = CyclesPerSecond;
	}

	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->bOscillationEnabled = true;
		EffectFrag->OscillationOffsetA = LocalOffsetA;
		EffectFrag->OscillationOffsetB = LocalOffsetB;
		EffectFrag->OscillationCyclesPerSecond = CyclesPerSecond;
		EffectFrag->OscillationDuration = Duration;
		EffectFrag->OscillationElapsed = 0.f;
		UInstancedStaticMeshComponent* ResolvedISM = Cast<UInstancedStaticMeshComponent>(ResolveVisualComponent());
		EffectFrag->OscillationTargetISM = ResolvedISM ? ResolvedISM : ISMComponent;
	}
}

void AConstructionUnit::KillConstructionUnit()
{
	if (HasAuthority())
	{
		// Perform immediately on the server
		SwitchEntityTagByState(UnitData::Dead, UnitData::Dead);
		SetHealth(0.f);
		SetHidden(true);
	}
	else
	{
		// Request the server to execute authoritative changes
		Server_KillConstructionUnit();
	}
}

void AConstructionUnit::Server_KillConstructionUnit_Implementation()
{
	SwitchEntityTagByState(UnitData::Dead, UnitData::Dead);
	SetHealth(0.f);
	SetHidden(true);
}

void AConstructionUnit::MulticastPulsateScale_Implementation(const FVector& MinMultiplier, const FVector& MaxMultiplier, float TimeMinToMax, bool bEnable)
{
	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->bPulsateEnabled = bEnable;
		
		// For AConstructionUnit, we often want to pulsate around the base scale (e.g. WorkArea fit)
		// Our processor handles this by multiplying current relative scale.
		// So we calculate Min/Max scale based on current base scale.
		
		UInstancedStaticMeshComponent* TargetISM = Cast<UInstancedStaticMeshComponent>(ResolveVisualComponent());
		if (!TargetISM) TargetISM = ISMComponent;
		
		FVector BaseScale = GetCurrentLocalVisualScale(TargetISM);
		
		EffectFrag->PulsateMinScale = BaseScale * MinMultiplier;
		EffectFrag->PulsateMaxScale = BaseScale * MaxMultiplier;
		EffectFrag->PulsateHalfPeriod = TimeMinToMax;
		EffectFrag->PulsateElapsed = 0.f;
		EffectFrag->PulsateTargetISM = TargetISM;

		if (HasAuthority())
		{
			if (bEnable) Rep_VE_ActiveEffects |= (1 << 0); else Rep_VE_ActiveEffects &= ~(1 << 0);
			Rep_VE_PulsateMinScale = EffectFrag->PulsateMinScale;
			Rep_VE_PulsateMaxScale = EffectFrag->PulsateMaxScale;
			Rep_VE_PulsateHalfPeriod = EffectFrag->PulsateHalfPeriod;
		}
	}
}
