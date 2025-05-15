// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Unit/MassUnitBase.h"

AMassUnitBase::AMassUnitBase(const FObjectInitializer& ObjectInitializer)
{
		
	// Create the Mass Actor Binding Component
	MassActorBindingComponent = CreateDefaultSubobject<UMassActorBindingComponent>(TEXT("MassActorBindingComponent"));
	if (MassActorBindingComponent)
	{
		// Attach it to the root component (optional, depending on your needs)
		MassActorBindingComponent->SetupAttachment(RootComponent);

		ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
		ISMComponent->SetupAttachment(RootComponent);
		ISMComponent->SetVisibility(false);
	}
}

bool AMassUnitBase::AddTagToEntity(UScriptStruct* TagToAdd)
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
    //UE_LOG(LogTemp, Verbose, TEXT("ASpawnerUnit (%s): Attempted to add tag '%s' to entity %s."), *GetName(), *TagToAdd->GetName(), *EntityHandle.DebugGetDescription());

    // AddTagToEntity doesn't return success/failure, but we assume it worked if we got this far.
    // If you need strict confirmation, you'd check HasTag afterwards, but that adds overhead.
    return true;
}

bool AMassUnitBase::RemoveTagFromEntity(UScriptStruct* TagToRemove)
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
    //UE_LOG(LogTemp, Verbose, TEXT("ASpawnerUnit (%s): Attempted to remove tag '%s' from entity %s."), *GetName(), *TagToRemove->GetName(), *EntityHandle.DebugGetDescription());

    // Like AddTag, RemoveTag doesn't return status. Assume success if code reached here.
    return true;
}

bool AMassUnitBase::SwitchEntityTag(UScriptStruct* TagToAdd)
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


	FMassAIStateFragment* StateFrag = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle);
	StateFrag->StateTimer = 0.f;
	UnitControlTimer = 0.f;
		
	if      (TagToAdd == FMassStateIdleTag::StaticStruct())                 SetUnitState(UnitData::Idle);
	else if (TagToAdd == FMassStateChaseTag::StaticStruct())                SetUnitState(UnitData::Chase);
	else if (TagToAdd == FMassStateAttackTag::StaticStruct())               SetUnitState(UnitData::Attack);
	else if (TagToAdd == FMassStatePauseTag::StaticStruct())                SetUnitState(UnitData::Pause);
	else if (TagToAdd == FMassStateDeadTag::StaticStruct())                 SetUnitState(UnitData::Dead);
	else if (TagToAdd == FMassStateRunTag::StaticStruct())                  SetUnitState(UnitData::Run);
	else if (TagToAdd == FMassStatePatrolRandomTag::StaticStruct())         SetUnitState(UnitData::PatrolRandom);
	else if (TagToAdd == FMassStatePatrolIdleTag::StaticStruct())           SetUnitState(UnitData::PatrolIdle);
	else if (TagToAdd == FMassStateCastingTag::StaticStruct())              SetUnitState(UnitData::Casting);
	else if (TagToAdd == FMassStateIsAttackedTag::StaticStruct())           SetUnitState(UnitData::IsAttacked);
	else if (TagToAdd == FMassStateGoToBaseTag::StaticStruct())             SetUnitState(UnitData::GoToBase);
	else if (TagToAdd == FMassStateGoToBuildTag::StaticStruct())            SetUnitState(UnitData::GoToBuild);
	else if (TagToAdd == FMassStateBuildTag::StaticStruct())                SetUnitState(UnitData::Build);
	else if (TagToAdd == FMassStateGoToResourceExtractionTag::StaticStruct()) SetUnitState(UnitData::GoToResourceExtraction);
	else if (TagToAdd == FMassStateResourceExtractionTag::StaticStruct())   SetUnitState(UnitData::ResourceExtraction);
	
	
	EntityManager->AddTagToEntity(EntityHandle, TagToAdd);
	// Like AddTag, RemoveTag doesn't return status. Assume success if code reached here.
	return true;
}

bool AMassUnitBase::GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle)
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

void AMassUnitBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeUnitMode();
}

void AMassUnitBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Only add once (or whenever mesh/flag changes)
	if (!bUseSkeletalMovement 
		&& ISMComponent 
		&& ISMComponent->GetStaticMesh() 
		&& InstanceIndex == INDEX_NONE)
	{
		// This is still early enough that the ISM data manager
		// will allow an append from INDEX_NONE
		ISMComponent->ClearInstances();
		InstanceIndex = ISMComponent->AddInstance(Transform, /*bWorldSpace=*/true);
	}
}

void AMassUnitBase::InitializeUnitMode()
{
	if (bUseSkeletalMovement)
	{
		GetMesh()->SetVisibility(true);
		ISMComponent->SetVisibility(false);
	}
	else
	{
		GetMesh()->SetVisibility(false);
		ISMComponent->SetVisibility(true);
	}
}

bool AMassUnitBase::SyncTranslation()
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle   EntityHandle;

	// Grab our Mass entity data
	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		// already logged in GetMassEntityData
		return false;
	}

	// Make sure it’s still alive
	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("AMassUnitBase (%s): SyncTranslation failed – entity %s invalid."),
			   *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}

	// Fetch the transform fragment and overwrite its translation
	FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (!TransformFrag)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("AMassUnitBase (%s): SyncTranslation failed – no FTransformFragment found."),
			   *GetName());
		return false;
	}

	// Sync
	FTransform& Current = TransformFrag->GetMutableTransform();
	Current.SetTranslation(GetActorLocation());

	return true;
}
