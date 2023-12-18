// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Unit/SpawnerUnit.h"

void ASpawnerUnit::CreateSpawnDataFromDataTable()
{
	if (SpawnDataTable)
	{
		SpawnDataArray.Empty();

		TArray<FName> RowNames = SpawnDataTable->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			FSpawnData* RowData = SpawnDataTable->FindRow<FSpawnData>(RowName, FString(""));
			if (RowData)
			{
				SpawnDataArray.Add(*RowData);
			}
		}
	}
}

ASelectable* ASpawnerUnit::SpawnSelectable(FVector Location, TSubclassOf<ASelectable> SelectableClass)
{
	// Spawn a new instance of the ASelectable class
	ASelectable* NewSelectable = GetWorld()->SpawnActor<ASelectable>(SelectableClass, Location, FRotator::ZeroRotator);

	return NewSelectable;
}

bool ASpawnerUnit::SpawnSelectableWithProbability(FSpawnData Data, FVector Offset)
{
	float RandomNumber = FMath::RandRange(0.f, 100.f);

	if (Data.ProbabilityArray >= RandomNumber)
	{
		SpawnSelectable(GetActorLocation() + Offset, Data.SelectableBlueprints);
		return true;
	}

	return false;
}

void ASpawnerUnit::SpawnSelectablesArray()
{
	int i = 0;
	FVector Offset = FVector(5.f,5.f,0.f);
	for (const FSpawnData& Data : SpawnDataArray)
	{
		SpawnSelectableWithProbability(Data, Offset*i);
		i++;
	}
}