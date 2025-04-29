// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/SpawnerUnit.h"
#include "Actors/AbilityIndicator.h"
#include "Net/UnrealNetwork.h"

ASpawnerUnit::ASpawnerUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// Create the Mass Actor Binding Component
	MassActorBindingComponent = CreateDefaultSubobject<UMassActorBindingComponent>(TEXT("MassActorBindingComponent"));
	if (MassActorBindingComponent)
	{
		// Attach it to the root component (optional, depending on your needs)
		MassActorBindingComponent->SetupAttachment(RootComponent);
	}
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
	// Spawn a new instance of the ASelectable class
	//ASelectable* NewSelectable = GetWorld()->SpawnActor<ASelectable>(SelectableClass, Location, FRotator::ZeroRotator);

	//return NewSelectable;

	// Spawn a new instance of the ASelectable class with deferred spawn method
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

		}
	}
}

void ASpawnerUnit::DespawnCurrentAbilityIndicator()
{
	if (CurrentDraggedAbilityIndicator)
	{
		UE_LOG(LogTemp, Log, TEXT("Despawning current dragged ability indicator for actor: %s."), *GetName());
		CurrentDraggedAbilityIndicator->Destroy(true, true);
		CurrentDraggedAbilityIndicator = nullptr;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No current dragged ability indicator to despawn for actor: %s."), *GetName());
	}
}

bool ASpawnerUnit::AddTagToEntity(UScriptStruct* TagToAdd)
{
    if (!TagToAdd || !TagToAdd->IsChildOf(FMassTag::StaticStruct()))
    {
        UE_LOG(LogTemp, Error, TEXT("ASpawnerUnit (%s): AddTagToEntity failed - Invalid Tag ScriptStruct provided."), *GetName());
        return false;
    }

    FMassEntityManager* EntityManager;
    FMassEntityHandle EntityHandle;

    if (!GetMassEntityData(EntityManager, EntityHandle))
    {
        // Error already logged in GetMassEntityData
        return false;
    }

    // Check if entity is still valid before attempting modification
    if (!EntityManager->IsEntityValid(EntityHandle))
    {
         UE_LOG(LogTemp, Warning, TEXT("ASpawnerUnit (%s): AddTagToEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
         return false;
    }

    // Add the tag
    EntityManager->AddTagToEntity(EntityHandle, TagToAdd);
    UE_LOG(LogTemp, Verbose, TEXT("ASpawnerUnit (%s): Attempted to add tag '%s' to entity %s."), *GetName(), *TagToAdd->GetName(), *EntityHandle.DebugGetDescription());

    // AddTagToEntity doesn't return success/failure, but we assume it worked if we got this far.
    // If you need strict confirmation, you'd check HasTag afterwards, but that adds overhead.
    return true;
}

bool ASpawnerUnit::RemoveTagFromEntity(UScriptStruct* TagToRemove)
{
    if (!TagToRemove || !TagToRemove->IsChildOf(FMassTag::StaticStruct()))
    {
        UE_LOG(LogTemp, Error, TEXT("ASpawnerUnit (%s): RemoveTagFromEntity failed - Invalid Tag ScriptStruct provided."), *GetName());
        return false;
    }

    FMassEntityManager* EntityManager;
    FMassEntityHandle EntityHandle;

    if (!GetMassEntityData(EntityManager, EntityHandle))
    {
        // Error already logged in GetMassEntityData
        return false;
    }

    // Check if entity is still valid
    if (!EntityManager->IsEntityValid(EntityHandle))
    {
         UE_LOG(LogTemp, Warning, TEXT("ASpawnerUnit (%s): RemoveTagFromEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
         return false;
    }

    // Remove the tag
    EntityManager->RemoveTagFromEntity(EntityHandle, TagToRemove);
    UE_LOG(LogTemp, Verbose, TEXT("ASpawnerUnit (%s): Attempted to remove tag '%s' from entity %s."), *GetName(), *TagToRemove->GetName(), *EntityHandle.DebugGetDescription());

    // Like AddTag, RemoveTag doesn't return status. Assume success if code reached here.
    return true;
}

bool ASpawnerUnit::SwitchEntityTag(UScriptStruct* TagToAdd)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		// Error already logged in GetMassEntityData
		return false;
	}

	// Check if entity is still valid
	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("ASpawnerUnit (%s): RemoveTagFromEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}

	
	// Remove the tag
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateIdleTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateChaseTag::StaticStruct());

	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateAttackTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStatePauseTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateDeadTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateRunTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStatePatrolRandomTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStatePatrolIdleTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateCastingTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateIsAttackedTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateGoToBaseTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateGoToBuildTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateBuildTag::StaticStruct());

	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateGoToResourceExtractionTag::StaticStruct());
	EntityManager->RemoveTagFromEntity(EntityHandle, FMassStateResourceExtractionTag::StaticStruct());

	EntityManager->AddTagToEntity(EntityHandle, TagToAdd);
	// Like AddTag, RemoveTag doesn't return status. Assume success if code reached here.
	return true;
}

bool ASpawnerUnit::GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle)
{
	OutEntityManager = nullptr;
	OutEntityHandle.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ASpawnerUnit (%s): Cannot get Mass Entity Data - World is invalid."), *GetName());
		return false;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("ASpawnerUnit (%s): Cannot get Mass Entity Data - UMassEntitySubsystem is invalid."), *GetName());
		return false;
	}

	if (!MassActorBindingComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ASpawnerUnit (%s): Cannot get Mass Entity Data - MassActorBindingComponent is missing."), *GetName());
		return false;
	}

	OutEntityHandle = MassActorBindingComponent->GetEntityHandle();
	if (!OutEntityHandle.IsSet())
	{
		// This might be expected if the entity hasn't been registered yet, so using Warning.
		UE_LOG(LogTemp, Warning, TEXT("ASpawnerUnit (%s): Cannot get Mass Entity Data - Entity Handle is not set in Binding Component."), *GetName());
		return false;
	}

	OutEntityManager = &EntitySubsystem->GetMutableEntityManager(); // Get mutable manager for modifications
	return true;
}