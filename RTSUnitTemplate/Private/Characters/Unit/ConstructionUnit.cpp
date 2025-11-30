// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Unit/ConstructionUnit.h"
#include "Net/UnrealNetwork.h"
#include "Actors/WorkArea.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Core/UnitData.h"

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

void AConstructionUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RotateTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(OscillateTimerHandle);
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
	Rotate_Axis = Axis.GetSafeNormal().IsNearlyZero() ? FVector(0,0,1) : Axis.GetSafeNormal();
	Rotate_DegreesPerSec = DegreesPerSecond;
	Rotate_Duration = FMath::Max(0.f, Duration);
	Rotate_Elapsed = 0.f;
	Rotate_TargetComp = ResolveVisualComponent();
	Rotate_UseActor = !Rotate_TargetComp.IsValid();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RotateTimerHandle);
		// 60 Hz update
		GetWorld()->GetTimerManager().SetTimer(RotateTimerHandle, this, &AConstructionUnit::RotateVisual_Step, 1.f/60.f, true);
	}
}

void AConstructionUnit::RotateVisual_Step()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Step = 1.f/60.f; // matches timer rate
	Rotate_Elapsed += Step;
	const float AngleDeg = Rotate_DegreesPerSec * Step;
	const FQuat DeltaQ(Rotate_Axis, FMath::DegreesToRadians(AngleDeg));

	if (Rotate_UseActor)
	{
		FQuat NewRot = DeltaQ * GetActorQuat();
		SetActorRotation(NewRot);
	}
	else if (USceneComponent* Comp = Rotate_TargetComp.Get())
	{
		FQuat NewRot = DeltaQ * Comp->GetRelativeRotation().Quaternion();
		Comp->SetRelativeRotation(NewRot);
	}

	if (Rotate_Elapsed >= Rotate_Duration)
	{
		World->GetTimerManager().ClearTimer(RotateTimerHandle);
	}
}

void AConstructionUnit::MulticastStartOscillateVisual_Implementation(const FVector& LocalOffsetA, const FVector& LocalOffsetB, float CyclesPerSecond, float Duration)
{
	Osc_OffsetA = LocalOffsetA;
	Osc_OffsetB = LocalOffsetB;
	Osc_CyclesPerSec = CyclesPerSecond;
	Osc_Duration = FMath::Max(0.f, Duration);
	Osc_Elapsed = 0.f;
	Osc_TargetComp = ResolveVisualComponent();
	Osc_UseActor = !Osc_TargetComp.IsValid();

	if (Osc_UseActor)
	{
		Osc_BaseActorLoc = GetActorLocation();
	}
	else if (USceneComponent* Comp = Osc_TargetComp.Get())
	{
		Osc_BaseRelativeLoc = Comp->GetRelativeLocation();
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(OscillateTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(OscillateTimerHandle, this, &AConstructionUnit::OscillateVisual_Step, 1.f/60.f, true);
	}
}

void AConstructionUnit::OscillateVisual_Step()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Step = 1.f/60.f;
	Osc_Elapsed += Step;

	// Smooth oscillation between A and B using cosine
	const float Omega = 2.f * PI * Osc_CyclesPerSec;
	const float Alpha = 0.5f * (1.f - FMath::Cos(Omega * Osc_Elapsed)); // 0..1..0..
	const FVector TargetLocal = FMath::Lerp(Osc_OffsetA, Osc_OffsetB, Alpha);

	if (Osc_UseActor)
	{
		SetActorLocation(Osc_BaseActorLoc + TargetLocal);
	}
	else if (USceneComponent* Comp = Osc_TargetComp.Get())
	{
		Comp->SetRelativeLocation(Osc_BaseRelativeLoc + TargetLocal);
	}

	if (Osc_Elapsed >= Osc_Duration)
	{
		World->GetTimerManager().ClearTimer(OscillateTimerHandle);
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
