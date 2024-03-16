// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/HealingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AHealingActor::AHealingActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // QueryAndPhysics
	Mesh->SetMaterial(0, Material);
	Mesh->SetSimulatePhysics(false);
	
	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Is PHealing Capsule"));
	TriggerCapsule->InitCapsuleSize(100.f, 100.0f);;
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));

	TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &AHealingActor::OnOverlapBegin);

	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
	}
}

void AHealingActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHealingActor, Mesh);
	DOREPLIFETIME(AHealingActor, Material);

	
}

void AHealingActor::Init(AUnitBase* Target, AUnitBase* Healer)
{
	HealingUnit = Healer;
	Target->SetHealth(Target->Attributes->GetHealth() + MainHeal);
	Healer->LevelData.Experience++;
}

// Called when the game starts or when spawned
void AHealingActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHealingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ControlTimer = ControlTimer + DeltaTime;
	if(ControlTimer >= HealIntervalTime){
		ControlTimer = 0.f;
		ControlInterval++;
		if(ControlInterval > MaxIntervals) Destroy(true);
		
		for (int32 i = 0; i < Actors.Num(); i++)
		{
			if(Actors[i] && Actors[i]->GetUnitState() != UnitData::Dead)
			Actors[i]->SetHealth(Actors[i]->Attributes->GetHealth() + IntervalHeal);
		}
	}
}


void AHealingActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{
		AUnitBase* UnitToHeal = Cast<AUnitBase>(OtherActor);

		if(UnitToHeal && HealingUnit && HealingUnit->TeamId == UnitToHeal->TeamId && UnitToHeal->GetUnitState() != UnitData::Dead)
			Actors.Push(UnitToHeal);
			
	}
}
