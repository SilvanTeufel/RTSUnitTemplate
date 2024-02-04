// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/WorkArea.h"

#include "Characters/Unit/UnitBase.h"
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
		AUnitBase* UnitBase = Cast<AUnitBase>(OtherActor);
		if(UnitBase && UnitBase->GetUnitState() == UnitData::GoToResourceExtraction)
		{
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUnitState(UnitData::ResourceExtraction);
			UE_LOG(LogTemp, Warning, TEXT("Overlap switch to -> ResourceExtraction"));
		}else if(UnitBase && UnitBase->GetUnitState() == UnitData::GoToBase && IsBase)
		{
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;
			DespawnWorkResource(UnitBase->WorkResource);
			UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
			UE_LOG(LogTemp, Warning, TEXT("Overlap switch to -> GoToResourceExtraction"));
		}
		/*
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffect);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			UnitToHit->ApplyInvestmentEffect(AreaEffect);
		}*/
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