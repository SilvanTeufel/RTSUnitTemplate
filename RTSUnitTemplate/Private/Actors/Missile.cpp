// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/Missile.h"

#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"

// Sets default values
AMissile::AMissile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);
}

// Called when the game starts or when spawned
void AMissile::BeginPlay()
{
	Super::BeginPlay();
	
}


void AMissile::Init(int TId)
{
	TeamId = TId;
}

// Called every frame
void AMissile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	LifeTime+=DeltaTime;
	
	if(LifeTime >= MaxLifeTime){
		Destroy(true, false);
	}else
	{
		FVector ALoc = GetActorLocation();
		SetActorLocation(FVector(ALoc.X+Velocity.X, ALoc.Y+Velocity.Y, ALoc.Z+Velocity.Z));
	}
}


void AMissile::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			Destroy(true, false);
		}else if(UnitToHit && UnitToHit->TeamId != TeamId)
		{
			if(UnitToHit->Attributes->GetShield() <= 0)
				UnitToHit->SetHealth_Implementation(UnitToHit->Attributes->GetHealth()-(Damage - UnitToHit->Attributes->GetMagicResistance()));
			else
				UnitToHit->SetShield_Implementation(UnitToHit->Attributes->GetShield()-(Damage - UnitToHit->Attributes->GetMagicResistance()));
			
			if(UnitToHit->GetUnitState() != UnitData::Run &&
				UnitToHit->GetUnitState() != UnitData::Attack &&
				UnitToHit->GetUnitState() != UnitData::Casting &&
				UnitToHit->GetUnitState() != UnitData::Rooted &&
				UnitToHit->GetUnitState() != UnitData::Pause)
			{
				UnitToHit->SetUnitState( UnitData::IsAttacked );
				
				UWorld* World = GetWorld();
				
				if (!World) return;

				
				UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(World);

				if (!SignalSubsystem) return;
				
				FMassEntityHandle MassEntityHandle =  UnitToHit->MassActorBindingComponent->GetMassEntityHandle();
				SignalSubsystem->SignalEntity(UnitSignals::IsAttacked, MassEntityHandle);
			}
			Destroy(true, false);
		}
			
	}
}
