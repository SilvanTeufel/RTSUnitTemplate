// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/HealingUnit.h"
#include "Actors/HealingActor.h"
#include "Kismet/GameplayStatics.h"

void AHealingUnit::SpawnHealActor(AActor* Target) // FVector TargetLocation
{


	if (HealingActorBaseClass)
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

	// Entferne alle Einheiten, die ungültig, tot oder außerhalb der Sichtweite sind.
	UnitsToChase.RemoveAll([this](const AUnitBase* Unit) -> bool {
		return !IsValid(Unit) || Unit->GetUnitState() == UnitData::Dead || GetDistanceTo(Unit) > MassActorBindingComponent->SightRadius;
	});
	
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