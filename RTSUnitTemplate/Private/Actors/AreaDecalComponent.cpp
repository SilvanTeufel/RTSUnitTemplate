// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AreaDecalComponent.h" // Adjust path if necessary

#include "Characters/Unit/UnitBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

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


	AUnitBase* OwningUnit = Cast<AUnitBase>(GetOwner());

	// 2. CRITICAL: Check if the cast was successful before using the pointer!
	if (!OwningUnit)
	{
		UE_LOG(LogTemp, Error, TEXT("AreaDecalComponent's owner '%s' is not an AUnitBase! Cannot interact with Mass."), *GetOwner()->GetName());
		return;
	}

	bool bVisibility = OwningUnit->ComputeLocalVisibility();

	SetHiddenInGame(!bVisibility);
	SetVisibility(bVisibility);
	
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
	
	const FString RoleString = GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client");
	
	// If the material is null, the decal should be hidden.
	if (!CurrentMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] AreaDecalComponent on %s: Hiding decal because CurrentMaterial is null."), *RoleString, *GetOwner()->GetName());
		SetHiddenInGame(true);
		return;
	}

	// Create a dynamic material instance if we don't have one or if the base material has changed.
	if (!DynamicDecalMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] AreaDecalComponent on %s: Creating new Dynamic Material Instance from %s."), *RoleString, *GetOwner()->GetName(), *CurrentMaterial->GetName());
		DynamicDecalMaterial = UMaterialInstanceDynamic::Create(CurrentMaterial, this);
		SetDecalMaterial(DynamicDecalMaterial);
	}

	if (DynamicDecalMaterial)
	{
		// Apply the replicated color. Your material needs a Vector Parameter named "Color".
		DynamicDecalMaterial->SetVectorParameterValue("Color", CurrentDecalColor);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] AreaDecalComponent on %s: DynamicDecalMaterial is still null after creation attempt."), *RoleString, *GetOwner()->GetName());
	}
	
	// Apply the replicated radius. Z is the projection depth, Y and Z are the radius.
	DecalSize = FVector(DecalSize.X, CurrentDecalRadius/3.f, CurrentDecalRadius/3.f);

	// Ensure the decal is visible.
	UE_LOG(LogTemp, Error, TEXT("[%s] AreaDecalComponent on %s: Updating and showing decal. Color: %s, Radius: %.2f"), *RoleString, *GetOwner()->GetName(), *CurrentDecalColor.ToString(), CurrentDecalRadius);
	
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
	// Setting the material to null is the signal for deactivation.
	CurrentMaterial = nullptr;

	// The server hides it immediately.
	SetHiddenInGame(true);

	// OnRep_CurrentMaterial will be called on clients, which will then also hide it.
}



