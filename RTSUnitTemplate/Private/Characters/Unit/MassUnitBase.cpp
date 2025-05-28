// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Unit/MassUnitBase.h"

#include "Mass/Signals/MySignals.h"
#include "Net/UnrealNetwork.h"

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
		ISMComponent->SetIsReplicated(true);
	}
}

void AMassUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMassUnitBase, ISMComponent);
	DOREPLIFETIME(AMassUnitBase, bUseSkeletalMovement);
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

bool AMassUnitBase::SwitchEntityTagByState(TEnumAsByte<UnitData::EState> UState, TEnumAsByte<UnitData::EState> UStatePlaceholder)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		// Error already logged in GetMassEntityData
		UE_LOG(LogTemp, Warning, TEXT("!!!NO ENITY OR MANGER FOUND!!!"));
	
		return false;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): SwitchEntityTagByState failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}

	// Remove all state tags
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

	// Reset state timers
	FMassAIStateFragment* StateFrag = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle);
	StateFrag->StateTimer = 0.f;
	UnitControlTimer = 0.f;

	// Set PlaceholderSignal using UnitStatePlaceholder
	switch (UnitStatePlaceholder)
	{
	case UnitData::Idle: StateFrag->PlaceholderSignal = UnitSignals::Idle; break;
	case UnitData::Chase: StateFrag->PlaceholderSignal = UnitSignals::Chase; break;
	case UnitData::Attack: StateFrag->PlaceholderSignal = UnitSignals::Attack; break;
	case UnitData::Pause: StateFrag->PlaceholderSignal = UnitSignals::Pause; break;
	case UnitData::Dead: StateFrag->PlaceholderSignal = UnitSignals::Dead; break;
	case UnitData::Run: StateFrag->PlaceholderSignal = UnitSignals::Run; break;
	case UnitData::PatrolRandom: StateFrag->PlaceholderSignal = UnitSignals::PatrolRandom; break;
	case UnitData::PatrolIdle: StateFrag->PlaceholderSignal = UnitSignals::PatrolIdle; break;
	case UnitData::Casting: StateFrag->PlaceholderSignal = UnitSignals::Casting; break;
	case UnitData::IsAttacked: StateFrag->PlaceholderSignal = UnitSignals::IsAttacked; break;
	case UnitData::GoToBase: StateFrag->PlaceholderSignal = UnitSignals::GoToBase; break;
	case UnitData::GoToBuild: StateFrag->PlaceholderSignal = UnitSignals::GoToBuild; break;
	case UnitData::Build: StateFrag->PlaceholderSignal = UnitSignals::Build; break;
	case UnitData::GoToResourceExtraction: StateFrag->PlaceholderSignal = UnitSignals::GoToResourceExtraction; break;
	case UnitData::ResourceExtraction: StateFrag->PlaceholderSignal = UnitSignals::ResourceExtraction; break;
	default: 
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): Invalid UnitStatePlaceholder."), *GetName());
		break;
	}

	// Determine tag from UnitState
	UScriptStruct* NewTag = nullptr;
	switch (UnitState)
	{
	case UnitData::Idle: NewTag = FMassStateIdleTag::StaticStruct(); break;
	case UnitData::Chase: NewTag = FMassStateChaseTag::StaticStruct(); break;
	case UnitData::Attack: NewTag = FMassStateAttackTag::StaticStruct(); break;
	case UnitData::Pause: NewTag = FMassStatePauseTag::StaticStruct(); break;
	case UnitData::Dead: NewTag = FMassStateDeadTag::StaticStruct(); break;
	case UnitData::Run: NewTag = FMassStateRunTag::StaticStruct(); break;
	case UnitData::PatrolRandom: NewTag = FMassStatePatrolRandomTag::StaticStruct(); break;
	case UnitData::PatrolIdle: NewTag = FMassStatePatrolIdleTag::StaticStruct(); break;
	case UnitData::Casting: NewTag = FMassStateCastingTag::StaticStruct(); break;
	case UnitData::IsAttacked: NewTag = FMassStateIsAttackedTag::StaticStruct(); break;
	case UnitData::GoToBase: NewTag = FMassStateGoToBaseTag::StaticStruct(); break;
	case UnitData::GoToBuild: NewTag = FMassStateGoToBuildTag::StaticStruct(); break;
	case UnitData::Build: NewTag = FMassStateBuildTag::StaticStruct(); break;
	case UnitData::GoToResourceExtraction: NewTag = FMassStateGoToResourceExtractionTag::StaticStruct(); break;
	case UnitData::ResourceExtraction: NewTag = FMassStateResourceExtractionTag::StaticStruct(); break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): Unknown UnitState."), *GetName());
		return false;
	}

	SetUnitState(UnitState);
	EntityManager->AddTagToEntity(EntityHandle, NewTag);

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

	if (GetUnitState() == UnitData::Idle)
		StateFrag->PlaceholderSignal = UnitSignals::Idle;
	else if (GetUnitState() == UnitData::Run)
		StateFrag->PlaceholderSignal = UnitSignals::Run;
	else if (GetUnitState() == UnitData::PatrolRandom)
		StateFrag->PlaceholderSignal = UnitSignals::PatrolRandom;
	else if (GetUnitState() == UnitData::PatrolIdle)
		StateFrag->PlaceholderSignal = UnitSignals::PatrolIdle;
	else if (GetUnitState() == UnitData::GoToResourceExtraction)
		StateFrag->PlaceholderSignal = UnitSignals::GoToResourceExtraction;
	//else if (GetUnitState() == UnitData::Chase)
		//StateFrag->PlaceholderSignal = UnitSignals::Chase;
	//else if (GetUnitState() == UnitData::Attack)
		//StateFrag->PlaceholderSignal = UnitSignals::Attack;
	//else if (GetUnitState() == UnitData::Pause)
		//StateFrag->PlaceholderSignal = UnitSignals::Pause;
	//else if (GetUnitState() == UnitData::Dead)
		//StateFrag->PlaceholderSignal = UnitSignals::Dead;
	//else if (GetUnitState() == UnitData::GoToBase)
		//StateFrag->PlaceholderSignal = UnitSignals::GoToBase;
	//else if (GetUnitState() == UnitData::GoToBuild)
		//StateFrag->PlaceholderSignal = UnitSignals::GoToBuild;
	//else if (GetUnitState() == UnitData::Build)
		//StateFrag->PlaceholderSignal = UnitSignals::Build;
	//else if (GetUnitState() == UnitData::ResourceExtraction)
		//StateFrag->PlaceholderSignal = UnitSignals::ResourceExtraction;
	
		
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

void AMassUnitBase::Multicast_UpdateISMInstanceTransform_Implementation(int32 InstIndex,
	const FTransform& NewTransform)
{
	if (ISMComponent && ISMComponent->IsValidInstance(InstIndex))
	{
		ISMComponent->UpdateInstanceTransform(InstIndex, NewTransform, true, false, false);
		//ISMComponent->MarkRenderStateDirty();
	}
}
