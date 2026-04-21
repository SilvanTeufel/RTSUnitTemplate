// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Signals/MassUnitSpawnerSubsystem.h"
#include "Characters/Unit/UnitBase.h"

void UMassUnitSpawnerSubsystem::RegisterUnitForMassCreation(AUnitBase* NewUnit)
{
	if (NewUnit) { PendingUnits.Add(NewUnit); }
}

void UMassUnitSpawnerSubsystem::GetAndClearPendingUnits(TArray<AUnitBase*>& OutPendingUnits)
{
	for (TObjectPtr<AUnitBase>& UnitPtr : PendingUnits)
	{
		if(UnitPtr.Get()) { OutPendingUnits.Add(UnitPtr.Get()); }
	}
	PendingUnits.Empty();
}

void UMassUnitSpawnerSubsystem::RegisterEffectAreaForMassCreation(AEffectArea* NewArea)
{
	if (NewArea) { PendingEffectAreas.Add(NewArea); }
}

void UMassUnitSpawnerSubsystem::GetAndClearPendingEffectAreas(TArray<AEffectArea*>& OutPendingAreas)
{
	for (TObjectPtr<AEffectArea>& AreaPtr : PendingEffectAreas)
	{
		if (AreaPtr.Get()) { OutPendingAreas.Add(AreaPtr.Get()); }
	}
	PendingEffectAreas.Empty();
}

void UMassUnitSpawnerSubsystem::ResetSystem()
{
	PendingUnits.Empty();
	PendingEffectAreas.Empty();
}