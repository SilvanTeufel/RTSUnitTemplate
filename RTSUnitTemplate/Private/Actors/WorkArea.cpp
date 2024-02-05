// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/WorkArea.h"

#include "Characters/Unit/UnitBase.h"
#include "GameModes/ResourceGameMode.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AWorkArea::AWorkArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);
	
	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
	}
}

// Called when the game starts or when spawned
void AWorkArea::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AWorkArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	
}

void AWorkArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorkArea, AreaEffect);
	DOREPLIFETIME(AWorkArea, Mesh);
}





void AWorkArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(OtherActor);
		AUnitBase* UnitBase = Cast<AUnitBase>(Worker);
		if(Worker && UnitBase &&  ( UnitBase->GetUnitState() == UnitData::GoToResourceExtraction || UnitBase->GetUnitState() == UnitData::Evasion) && Type != WorkAreaData::Base)
		{
			UnitBase->UnitControlTimer = 0;
			
			switch(Type)
			{
			case WorkAreaData::Primary: UnitBase->ExtractingWorkResourceType = EResourceType::Primary; break;
			case WorkAreaData::Secondary: UnitBase->ExtractingWorkResourceType = EResourceType::Secondary; break;
			case WorkAreaData::Tertiary: UnitBase->ExtractingWorkResourceType = EResourceType::Tertiary; break;
			case WorkAreaData::Rare: UnitBase->ExtractingWorkResourceType = EResourceType::Rare; break;
			case WorkAreaData::Epic: UnitBase->ExtractingWorkResourceType = EResourceType::Epic; break;
			case WorkAreaData::Legendary: UnitBase->ExtractingWorkResourceType = EResourceType::Legendary; break;
			default: break; // Optionally handle default case
			}
			
			UnitBase->SetUnitState(UnitData::ResourceExtraction);
			
		}else if(Worker && UnitBase && Type == WorkAreaData::Base)
		{
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;
			DespawnWorkResource(UnitBase->WorkResource);
			UnitBase->SetUnitState(UnitData::GoToResourceExtraction);

			AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
			if(!ResourceGameMode) return; // Exit if the cast fails or game mode is not set

			ResourceGameMode->ModifyResource(Worker->WorkResource->ResourceType, Worker->TeamId, Worker->WorkResource->Amount); // Assuming 1.0f as the resource amount to add
		}


	}
}

void AWorkArea::DespawnWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		WorkResource->Destroy();
	}
}