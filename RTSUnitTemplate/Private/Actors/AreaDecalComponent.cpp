// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AreaDecalComponent.h" // Adjust path if necessary

#include "Characters/Unit/UnitBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Characters/Unit/BuildingBase.h"

UAreaDecalComponent::UAreaDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = TickInterval; 
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
}

void UAreaDecalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Register properties for replication.
	DOREPLIFETIME(UAreaDecalComponent, CurrentMaterial);
	DOREPLIFETIME(UAreaDecalComponent, CurrentDecalColor);
	DOREPLIFETIME(UAreaDecalComponent, CurrentDecalRadius);
	DOREPLIFETIME(UAreaDecalComponent, bDecalIsVisible);
}

void UAreaDecalComponent::BeginPlay()
{
	Super::BeginPlay();
	SetHiddenInGame(true);
	
	UpdateDecalVisuals();

}

void UAreaDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDecalIsVisible) return;
	
	AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner());

	// 2. CRITICAL: Check if the cast was successful before using the pointer!
	if (!OwningUnit)
	{
		UE_LOG(LogTemp, Error, TEXT("AreaDecalComponent's owner '%s' is not an AUnitBase! Cannot interact with Mass."), *GetOwner()->GetName());
		return;
	}

	bool bVisibility = OwningUnit->ComputeLocalVisibility();

	// Only update visibility if the state needs to change to avoid redundant calls.
	if (IsVisible() != bVisibility)
	{
		SetVisibility(bVisibility);
		SetHiddenInGame(!bVisibility);
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
	UpdateDecalVisuals();
}

void UAreaDecalComponent::UpdateDecalVisuals()
{
	
	// If the material is null, the decal should be hidden.
	if (!CurrentMaterial)
	{
		SetHiddenInGame(true);
		return;
	}

	// Create a dynamic material instance if we don't have one or if the base material has changed.
	if (!DynamicDecalMaterial)
	{
		DynamicDecalMaterial = UMaterialInstanceDynamic::Create(CurrentMaterial, this);
		SetDecalMaterial(DynamicDecalMaterial);
	}

	if (DynamicDecalMaterial)
	{
		// Apply the replicated color. Your material needs a Vector Parameter named "Color".
		DynamicDecalMaterial->SetVectorParameterValue("Color", CurrentDecalColor);
	}
	
	// Apply the replicated radius. Z is the projection depth, Y and Z are the radius.
	DecalSize = FVector(DecalSize.X, CurrentDecalRadius, CurrentDecalRadius);


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
		UpdateMassEffectRadius(EndRadius);
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
	UpdateMassEffectRadius(NewRadius);
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



