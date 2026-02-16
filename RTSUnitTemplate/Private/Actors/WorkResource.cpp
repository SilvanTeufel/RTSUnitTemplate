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

	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Is Pickup Capsule"));
	TriggerCapsule->InitCapsuleSize(100.f, 100.0f);;
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));
	TriggerCapsule->SetupAttachment(RootComponent);
	TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &AWorkResource::OnOverlapBegin);
	
	if (HasAuthority())
	{
		bReplicates = true;
		//SetReplicateMovement(true);
	}
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
	if(FollowTarget && Target)
	{
		const FVector Direction = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation(), Target->GetActorLocation());
		AddActorWorldOffset(Direction * MovementSpeed);
		float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		if(Distance <= PickUpDistance)
		{
			AUnitBase* UnitBase = Cast<AUnitBase>(Target);
			
			if(!UnitBase) return;
			
			ImpactEvent();

			if(Sound)
				UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);


			AttachToComponent(Cast<USceneComponent>(UnitBase->GetMesh()), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("ResourceSocket"));
			//AttachToComponent(UnitBase->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("ResourceSocket"));
			FollowTarget = false;
			IsAttached = true;
		}
	}
}

void AWorkResource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AWorkResource, Tag);
	DOREPLIFETIME(AWorkResource, Mesh);
	DOREPLIFETIME(AWorkResource, PickUpDistance); // Added for Build
	DOREPLIFETIME(AWorkResource, FollowTarget); // Added for Build
	DOREPLIFETIME(AWorkResource, MovementSpeed); // Added for Build
	DOREPLIFETIME(AWorkResource, Target); // Added for Build
}




void AWorkResource::SetResourceActive(bool bActive, EResourceType Type, float InAmount, FVector Offset)
{
	ResourceType = Type;
	Amount = InAmount;
	SocketOffset = Offset;
	
	SetActorHiddenInGame(!bActive);
	
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

		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TriggerCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TriggerCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		// Reset state
		FollowTarget = false;
		Target = nullptr;
		IsAttached = false;
	}
}

void AWorkResource::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor && !FollowTarget)
	{

		AWorkingUnitBase* UnitBase = Cast<AWorkingUnitBase>(OtherActor);
		
		if(!UnitBase || IsAttached) return;
		//if(TeamId == UnitBase->TeamId) return;

		Target = UnitBase;
		FollowTarget = true;
			
	}
}
