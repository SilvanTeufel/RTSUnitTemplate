// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/EffectArea.h"


// Sets default values
AEffectArea::AEffectArea()
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
void AEffectArea::BeginPlay()
{
	Super::BeginPlay();


	GetWorld()->GetTimerManager().SetTimer(DamageTimerHandle, this, &AEffectArea::ApplyDamage, DamageIntervalTime, true);
}

void AEffectArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Clear the timer when the actor is destroyed or gameplay ends
	GetWorld()->GetTimerManager().ClearTimer(DamageTimerHandle);
}
// Called every frame
void AEffectArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTime+=DeltaTime;
	
	if(LifeTime >= MaxLifeTime){
		Destroy(true, false);
	}
	
}

void AEffectArea::ApplyDamage()
{
	for(AUnitBase* UnitToHit : UnitsToHit)
	{
		if(UnitToHit && !IsHealing) // Always check for null pointers before accessing
		{
			if(UnitToHit->Attributes->GetShield() <= 0)
				UnitToHit->SetHealth(UnitToHit->Attributes->GetHealth() - Damage);
			else
				UnitToHit->Attributes->SetShield(UnitToHit->Attributes->GetShield() - Damage);

			if(UnitToHit->GetUnitState() != UnitData::Run)
				UnitToHit->SetUnitState(UnitData::IsAttacked);
		}else if(UnitToHit && IsHealing) // Always check for null pointers before accessing
		{
			UnitToHit->SetHealth(UnitToHit->Attributes->GetHealth() + Damage);
		}
	}
}

void AEffectArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);
		
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			UnitsToHit.Remove(UnitToHit);
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			UnitsToHit.Add(UnitToHit);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			UnitsToHit.Add(UnitToHit);
		}
			
	}
}

void AEffectArea::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if(OtherActor)
	{
		AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);

		// If the unit is in the UnitsToHit array, remove it
		if(UnitToHit)
		{
			UnitsToHit.Remove(UnitToHit);
		}
	}
}