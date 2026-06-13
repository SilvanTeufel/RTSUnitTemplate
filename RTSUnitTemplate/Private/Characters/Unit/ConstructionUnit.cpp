// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Unit/ConstructionUnit.h"
#include "Net/UnrealNetwork.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "NiagaraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Core/UnitData.h"
#include "Mass/MassUnitVisualFragments.h"

AConstructionUnit::AConstructionUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Name = TEXT("ConstructionSite");
	// Likely stationary
	CanMove = false;
	bIsConstructionUnit = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AConstructionUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AConstructionUnit, Worker);
	DOREPLIFETIME(AConstructionUnit, WorkArea);
	DOREPLIFETIME(AConstructionUnit, Rep_VE_OscillationOffsetA);
	DOREPLIFETIME(AConstructionUnit, Rep_VE_OscillationOffsetB);
	DOREPLIFETIME(AConstructionUnit, Rep_VE_OscillationCyclesPerSecond);
	DOREPLIFETIME(AConstructionUnit, Rep_DroneStage);
	DOREPLIFETIME(AConstructionUnit, Rep_DroneTargetAngle);
	DOREPLIFETIME(AConstructionUnit, Rep_DroneTargetHeight);
	DOREPLIFETIME(AConstructionUnit, DroneBehavior);
	DOREPLIFETIME(AConstructionUnit, BuildingMaxHeight);
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

void AConstructionUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && DroneBehavior)
	{
		UpdateDroneLogic(DeltaSeconds);
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

void AConstructionUnit::UpdateDroneLogic(float DeltaSeconds)
{
	AWorkArea* WA = BuildArea ? BuildArea : WorkArea;
	if (!WA) return;

	// Determine Building Height
	if (WA->Mesh)
	{
		BuildingMaxHeight = WA->Mesh->Bounds.BoxExtent.Z * 2.f;
	}

	switch (Rep_DroneStage)
	{
	case 0: // Stage 1: Spawn far above and fly down
		if (DroneStateTimer == 0.f)
		{
			Rep_DroneStage = 0; // Ensure we are in 0
			Rep_DroneTargetHeight = BuildingMaxHeight * 0.5f;
		}
		
		// The actual movement happens in Mass processor
		// We just wait here or use a timer to transition if needed
		// But better let the Mass processor handle the progress and we just check state
		
		if (DroneStateTimer >= 3.0f) // Give it 3 seconds to "fly down"
		{
			Rep_DroneStage = 1; // Transition to Stage 2 (Orbit)
			DroneStateTimer = 0.f;
			Rep_DroneTargetAngle = FMath::RandRange(60.f, 180.f);
		}
		break;

	case 1: // Stage 2: Orbit around building
	{
		// In Stage 1 (Fragment DroneStage 1), it orbits.
		// We wait until it should have reached its target.
		// The speed is 45 deg/s.
		float EstimatedTime = Rep_DroneTargetAngle / 45.f;
		if (DroneStateTimer >= EstimatedTime)
		{
			Rep_DroneStage = 2; // Transition to Stage 3 (Stop and Focus)
			DroneStateTimer = 0.f;
		}
	}
	break;

	case 2: // Stage 3: Stop and rotate to building
	{
		if (DroneStateTimer >= 1.5f)
		{
			Rep_DroneStage = 3; // Transition to Stage 4 (Vertical move)
			DroneStateTimer = 0.f;
			Rep_DroneTargetHeight = FMath::RandRange(10.f, BuildingMaxHeight);
		}
	}
	break;

	case 3: // Stage 4: Fly up/down
		if (DroneStateTimer >= 2.0f) // 2 seconds for vertical move
		{
			Rep_DroneStage = 4; // Transition to Stage 5 (Reorient)
			DroneStateTimer = 0.f;
		}
		break;

	case 4: // Stage 5: Rotate back to movement direction
	{
		if (DroneStateTimer >= 1.0f)
		{
			Rep_DroneStage = 1; // Loop back to Orbit
			Rep_DroneTargetAngle += FMath::RandRange(60.f, 180.f);
			DroneStateTimer = 0.f;
		}
	}
	break;

	case 5: // Stage 6: Despawn (Fly up)
		// Stay in this stage until actor is destroyed
		break;
	}

	// Trigger Stage 5 Despawn for normal builds if build is almost done
	if (DroneBehavior && Rep_DroneStage < 5 && UnitState != UnitData::Casting)
	{
		const float BuildTimeLimit = WA->BuildTime;
		const float Remaining = BuildTimeLimit - WA->CurrentBuildTime;
		if (WA->CurrentBuildTime > BuildTimeLimit * 0.9f || Remaining < 2.0f)
		{
			Rep_DroneStage = 5;
			ForceNetUpdate();
		}
	}

	DroneStateTimer += DeltaSeconds;
}
