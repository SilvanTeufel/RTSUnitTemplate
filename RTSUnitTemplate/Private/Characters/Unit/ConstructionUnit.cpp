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
}

void AConstructionUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AConstructionUnit, Worker);
	DOREPLIFETIME(AConstructionUnit, WorkArea);
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
	}
}
