// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/SpawnerUnit.h"

#include "EngineUtils.h"
#include "Actors/AbilityIndicator.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Net/UnrealNetwork.h"

ASpawnerUnit::ASpawnerUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void ASpawnerUnit::BeginPlay()
{
	Super::BeginPlay();
	CreateSpawnDataFromDataTable();
}

void ASpawnerUnit::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ASpawnerUnit, TeamId);
	DOREPLIFETIME(ASpawnerUnit, SquadId);

	DOREPLIFETIME(ASpawnerUnit, UnitTags);
	DOREPLIFETIME(ASpawnerUnit, AbilitySelectionTag);
	DOREPLIFETIME(ASpawnerUnit, TalentTag);

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

APickup* ASpawnerUnit::SpawnPickup(FVector Location, FSpawnData Data)
{
	APickup* NewPickup = GetWorld()->SpawnActorDeferred<APickup>(Data.PickupBlueprint, FTransform(FRotator::ZeroRotator, Location));

	if (NewPickup)
	{
		// Set the TeamId of the NewSelectable
		NewPickup->TeamId = TeamId;
		NewPickup->MaxLifeTime = Data.MaxLifeTime;
		// Finish the spawning process after setting properties
		NewPickup->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
	}

	// Return the spawned Selectable
	return NewPickup;
}

bool ASpawnerUnit::SpawnPickupWithProbability(FSpawnData Data, FVector Offset)
{
	float RandomNumber = FMath::RandRange(0.f, 100.f);

	if (Data.ProbabilityArray >= RandomNumber)
	{
		SpawnPickup(GetActorLocation() + Offset, Data);
		return true;
	}

	return false;
}

void ASpawnerUnit::SpawnPickupsArray()
{
	int i = 0;
	FVector Offset = FVector(5.f,5.f,0.f);
	if(!IsSpawned)
	for (const FSpawnData& Data : SpawnDataArray)
	{
		SpawnPickupWithProbability(Data, Offset*i);
		i++;
	}
	IsSpawned = true;
}
