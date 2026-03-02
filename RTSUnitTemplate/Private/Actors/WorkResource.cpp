// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/WorkResource.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AWorkResource::AWorkResource()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);

	// Ensure the mesh component never replicates its own visibility state; we control it per-client
	if (Mesh)
	{
		Mesh->SetIsReplicated(false);
		Mesh->SetHiddenInGame(true);
		Mesh->SetVisibility(false, true);
	}
	
	bReplicates = true;
}

// Called when the game starts or when spawned
void AWorkResource::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AWorkResource::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWorkResource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AWorkResource, Tag);
	DOREPLIFETIME(AWorkResource, Mesh);
	DOREPLIFETIME(AWorkResource, IsAttached);
	DOREPLIFETIME(AWorkResource, ResourceType);
	DOREPLIFETIME(AWorkResource, Amount);
	DOREPLIFETIME(AWorkResource, SocketOffset);
}

void AWorkResource::OnRep_IsAttached()
{
	// Default to hidden to avoid client-side flash; per-client logic will reveal if needed
	if (Mesh)
	{
		Mesh->SetHiddenInGame(true);
		Mesh->SetVisibility(false, true);
	}
}

void AWorkResource::OnRep_ResourceType()
{
	// Update visuals, keep hidden by default; per-client code will show if visible
	if (Mesh)
	{
		if (ResourceMeshes.Contains(ResourceType))
		{
			Mesh->SetStaticMesh(ResourceMeshes[ResourceType]);
		}
		if (ResourceMaterials.Contains(ResourceType))
		{
			Mesh->SetMaterial(0, ResourceMaterials[ResourceType]);
		}
		Mesh->SetHiddenInGame(true);
		Mesh->SetVisibility(false, true);
	}
}




void AWorkResource::SetResourceActive(bool bActive, EResourceType Type, float InAmount, FVector Offset)
{
	ResourceType = Type;
	Amount = InAmount;
	SocketOffset = Offset;
	
	// No actor-level hiding. Let clients decide. However, default the mesh to hidden to avoid 1-frame flash on clients.
	if (Mesh)
	{
		Mesh->SetHiddenInGame(true);
		Mesh->SetVisibility(false, true);
	}
	
	if (bActive)
	{
		// Update Mesh and Material if available in the maps
		if (ResourceMeshes.Contains(Type))
		{
			Mesh->SetStaticMesh(ResourceMeshes[Type]);
		}
		
		if (ResourceMaterials.Contains(Type))
		{
			Mesh->SetMaterial(0, ResourceMaterials[Type]);
		}
		
	}
	else
	{
		IsAttached = false;
	}
}

