// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/HealingUnit.h"
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

bool AHealingUnit::SetNextUnitToChaseHeal()
{
	if (UnitsToChase.IsEmpty()) return false;

	AUnitBase* UnitWithLowestHealth = nullptr;
	float LowestHealth = FLT_MAX;

	// Use an iterator loop to be able to remove elements while iterating
	for (auto It = UnitsToChase.CreateIterator(); It; ++It)
	{
		AUnitBase* CurrentUnit = *It;
		if (CurrentUnit)
		{
			if (CurrentUnit->GetUnitState() == UnitData::Dead)
			{
				It.RemoveCurrent();
			}
			else if (CurrentUnit->Attributes->GetHealth() < CurrentUnit->Attributes->GetMaxHealth() &&
					 CurrentUnit->Attributes->GetHealth() < LowestHealth)
			{
				LowestHealth = CurrentUnit->Attributes->GetHealth();
				UnitWithLowestHealth = CurrentUnit;
			}
		}
	}

	if (UnitWithLowestHealth)
	{
		UnitToChase = UnitWithLowestHealth;
		return true;
	}

	return false;
}

/*

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
				UnitsToChase[i]->Attributes->GetHealth() < UnitsToChase[i]->Attributes->GetMaxHealth() &&
				UnitsToChase[i]->Attributes->GetHealth() < LowestHealth )
			{
				IndexLowestHealth = i;
				//UnitToChase = UnitsToChase[i];
				LowestHealth = UnitsToChase[i]->Attributes->GetHealth();
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
 */