// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Unit/ConstructionUnit.h"
#include "Net/UnrealNetwork.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "NiagaraComponent.h"
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

void AConstructionUnit::SetCharacterVisibility(bool desiredVisibility)
{
	Super::SetCharacterVisibility(desiredVisibility);
	if (WorkArea && WorkArea->Mesh)
	{
		WorkArea->Mesh->SetHiddenInGame(!desiredVisibility);
	}
}

void AConstructionUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RotateTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(OscillateTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(PulsateTimerHandle);
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


void AConstructionUnit::MulticastRotateNiagaraToOrigin_Implementation(UNiagaraComponent* NiagaraToRotate, const FRotator& RotationOffset, float InRotateDuration, float InRotationEaseExponent)
{
	if (!NiagaraToRotate || !WorkArea || !WorkArea->Origin)
	{
		return;
	}

	const FVector OriginLocation = WorkArea->Origin->GetActorLocation();
	const FVector NiagaraLocation = NiagaraToRotate->GetComponentLocation();
	const FVector Direction = OriginLocation - NiagaraLocation;

	// Calculate rotation to face the origin
	const FRotator FaceOriginRotation = Direction.Rotation();

	// Apply offset
	const FRotator TargetRotation = FaceOriginRotation + RotationOffset;

	// Call the base class function to handle the smooth rotation
	MulticastRotateNiagaraLinear(NiagaraToRotate, TargetRotation, InRotateDuration, InRotationEaseExponent);
}

// --- Pulsating scale (multiplicative on top of base scale) ---
void AConstructionUnit::MulticastPulsateScale_Implementation(const FVector& MinMultiplier, const FVector& MaxMultiplier, float TimeMinToMax, bool bEnable)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bEnable)
	{
		bPulsateActive = false;
		World->GetTimerManager().ClearTimer(PulsateTimerHandle);

		// Restore to captured base scale if we had one
		if (Pulsate_UseActor)
		{
			SetActorScale3D(Pulsate_BaseScale);
		}
		else if (USceneComponent* Comp = Pulsate_TargetComp.Get())
		{
			Comp->SetRelativeScale3D(Pulsate_BaseScale);
		}
		return;
	}

	// Enable and configure
	Pulsate_TargetComp = ResolveVisualComponent();
	Pulsate_UseActor = !Pulsate_TargetComp.IsValid();

	// Capture base scale (already includes WorkArea fit). We multiply our pulsation around this value.
	Pulsate_BaseScale = Pulsate_UseActor
		? GetActorScale3D()
		: (Pulsate_TargetComp.IsValid() ? Pulsate_TargetComp->GetRelativeScale3D() : GetActorScale3D());

	Pulsate_Min = MinMultiplier;
	Pulsate_Max = MaxMultiplier;
	Pulsate_HalfPeriod = FMath::Max(TimeMinToMax, 0.0001f);
	Pulsate_Elapsed = 0.f;
	bPulsateActive = true;

	// Start/update timer at 60 Hz and kick an immediate step
	World->GetTimerManager().ClearTimer(PulsateTimerHandle);
	PulsateScale_Step();
	World->GetTimerManager().SetTimer(PulsateTimerHandle, this, &AConstructionUnit::PulsateScale_Step, 1.f/60.f, true);
}

void AConstructionUnit::PulsateScale_Step()
{
	UWorld* World = GetWorld();
	if (!World || !bPulsateActive)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(PulsateTimerHandle);
		}
		return;
	}

	// Fixed step to match timer rate
	const float Step = 1.f/60.f;
	Pulsate_Elapsed += Step;

	// Ping-pong alpha 0->1->0 with half-period Pulsate_HalfPeriod
	const float Cycle = (Pulsate_Elapsed / Pulsate_HalfPeriod);
	float Mod = FMath::Fmod(Cycle, 2.f);
	if (Mod < 0.f) Mod += 2.f;
	const float Alpha = (Mod <= 1.f) ? Mod : (2.f - Mod);

	const FVector Mult = FMath::Lerp(Pulsate_Min, Pulsate_Max, Alpha);
	const FVector NewScale = Pulsate_BaseScale * Mult; // component-wise multiply

	if (Pulsate_UseActor)
	{
		SetActorScale3D(NewScale);
	}
	else if (USceneComponent* Comp = Pulsate_TargetComp.Get())
	{
		Comp->SetRelativeScale3D(NewScale);
	}
}
