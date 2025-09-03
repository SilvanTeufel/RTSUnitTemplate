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
	DOREPLIFETIME(ASpawnerUnit, CurrentDraggedAbilityIndicator);

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


void ASpawnerUnit::SpawnAbilityIndicator(TSubclassOf<AAbilityIndicator> AbilityIndicatorClass,
											   FVector SpawnLocation)
{

	AControllerBase* TeamControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
	
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		
		AController* PlayerController = It->Get();
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);


		
		if (ControllerBase)
		{

			if (ControllerBase->SelectableTeamId == TeamId)
			{
				if (ControllerBase->CurrentDraggedAbilityIndicator)
				{
					CurrentDraggedAbilityIndicator = ControllerBase->CurrentDraggedAbilityIndicator;
					CurrentDraggedAbilityIndicator->SetReplicateMovement(true);
					return;
				}

				TeamControllerBase = ControllerBase;
				
			}
		}
	}
	
	
	if (AbilityIndicatorClass)
	{


		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FRotator SpawnRotation = FRotator::ZeroRotator;

		// Spawn the replicated WorkArea on the server
		AAbilityIndicator* SpawnedAbilityIndicator = GetWorld()->SpawnActor<AAbilityIndicator>(
			AbilityIndicatorClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParams
		);

		if (SpawnedAbilityIndicator)
		{
			// Initialize any properties on the spawned WorkArea

			SpawnedAbilityIndicator->TeamId = TeamId;
			//SpawnedWorkArea->SceneRoot->SetVisibility(false, true);
			// Keep track of this WorkArea if needed
			CurrentDraggedAbilityIndicator = SpawnedAbilityIndicator;
			CurrentDraggedAbilityIndicator->SetReplicateMovement(true);

			if (TeamControllerBase->IsValidLowLevel())
			{
				TeamControllerBase->CurrentDraggedAbilityIndicator = CurrentDraggedAbilityIndicator;
			}
		}
	}
}

void ASpawnerUnit::DespawnCurrentAbilityIndicator()
{
	if (CurrentDraggedAbilityIndicator)
	{
		CurrentDraggedAbilityIndicator->Destroy(true, true);
		CurrentDraggedAbilityIndicator = nullptr;
	}

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		
		AController* PlayerController = It->Get();
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);


		
		if (ControllerBase)
		{

			if (ControllerBase->SelectableTeamId == TeamId)
			{
				if (ControllerBase->CurrentDraggedAbilityIndicator)
					ControllerBase->CurrentDraggedAbilityIndicator->Destroy(true, true);
				
				ControllerBase->CurrentDraggedAbilityIndicator = nullptr;
			}
		}
	}
}
