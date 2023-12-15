// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/HealingUnit.h"
#include "Actors/HealingActor.h"
#include "Kismet/GameplayStatics.h"

void AHealingUnit::SpawnHealActor(AActor* Target) // FVector TargetLocation
{



		FTransform Transform;
		Transform.SetLocation(Target->GetActorLocation() + HealActorSpawnOffset);
		Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator
		Transform.SetScale3D(HealActorScale);
		
		const auto MyHealActor = Cast<AHealingActor>
							(UGameplayStatics::BeginDeferredActorSpawnFromClass
							(this, HealingActorBaseClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
		if (MyHealActor != nullptr)
		{
		
			MyHealActor->Init(UnitToChase, this);
			UGameplayStatics::FinishSpawningActor(MyHealActor, Transform);
		}

}

bool AHealingUnit::SetNextUnitToChaseHeal()
{
	if(!UnitsToChase.Num()) return false;
	bool RValue = false;

	float LowestHealth = 999999.f;

	int IndexLowestHealth = 0;
	for(int i = 0; i < UnitsToChase.Num(); i++)
	{
		if(UnitsToChase[i] && UnitsToChase[i]->GetUnitState() != UnitData::Dead)
		{
			//DistanceToUnitToChase = GetDistanceTo(UnitsToChase[i]);

			if(
				UnitsToChase[i]->GetHealth() < UnitsToChase[i]->GetMaxHealth() &&
				UnitsToChase[i]->GetHealth() < LowestHealth )
			{
				IndexLowestHealth = i;
				//UnitToChase = UnitsToChase[i];
				LowestHealth = UnitsToChase[i]->GetHealth();
				RValue =  true;
			}
		}
	}

	if(UnitsToChase[IndexLowestHealth])UnitToChase = UnitsToChase[IndexLowestHealth];

	TArray <AUnitBase*> UnitsToDelete = UnitsToChase;
	
	for(int i = 0; i < UnitsToDelete.Num(); i++)
	{
		if(UnitsToDelete[i] && UnitsToDelete[i]->GetUnitState() == UnitData::Dead)
		{
			UnitsToChase.Remove(UnitsToDelete[i]);
		}
	}


	return RValue;
}

void AHealingUnit::ServerStartHealingEvent_Implementation()
{
	MultiCastStartHealingEvent();
}

bool AHealingUnit::ServerStartHealingEvent_Validate()
{
	return true;
}

void AHealingUnit::MultiCastStartHealingEvent_Implementation()
{
	StartHealingEvent();
}
