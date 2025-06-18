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