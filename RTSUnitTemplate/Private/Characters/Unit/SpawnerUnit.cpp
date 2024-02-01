// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/SpawnerUnit.h"

#include "Net/UnrealNetwork.h"

void ASpawnerUnit::BeginPlay()
{
	Super::BeginPlay();
	CreateSpawnDataFromDataTable();
}

void ASpawnerUnit::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ASpawnerUnit, TeamId);

}

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
	//ASelectable* NewSelectable = GetWorld()->SpawnActor<ASelectable>(SelectableClass, Location, FRotator::ZeroRotator);

	//return NewSelectable;

	// Spawn a new instance of the ASelectable class with deferred spawn method
	ASelectable* NewSelectable = GetWorld()->SpawnActorDeferred<ASelectable>(SelectableClass, FTransform(FRotator::ZeroRotator, Location));

	if (NewSelectable)
	{
		// Set the TeamId of the NewSelectable
		NewSelectable->TeamId = TeamId;

		// Finish the spawning process after setting properties
		NewSelectable->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
	}

	// Return the spawned Selectable
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
	if(!IsSpawned)
	for (const FSpawnData& Data : SpawnDataArray)
	{
		SpawnSelectableWithProbability(Data, Offset*i);
		i++;
	}
	IsSpawned = true;
}