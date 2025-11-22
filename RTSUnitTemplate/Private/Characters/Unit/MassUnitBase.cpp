// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Characters/Unit/MassUnitBase.h"

#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Mass/States/ChaseStateProcessor.h"
#include "MassCommonFragments.h"
#include "GameFramework/CharacterMovementComponent.h"
// For updating FMassMoveTargetFragment and prediction helpers
#include "Mass/UnitMassTag.h"
#include "MassNavigationFragments.h"
#include "TimerManager.h"

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

	
	SelectionIcon = CreateDefaultSubobject<USelectionDecalComponent>(TEXT("SelectionIcon"));

	if (SelectionIcon)
	{
		SelectionIcon->SetupAttachment(RootComponent);
	}
}

void AMassUnitBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AMassUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMassUnitBase, ISMComponent);
	DOREPLIFETIME(AMassUnitBase, bUseSkeletalMovement);
	DOREPLIFETIME(AMassUnitBase, bUseIsmWithActorMovement);

	DOREPLIFETIME(AMassUnitBase, Niagara_A);
	DOREPLIFETIME(AMassUnitBase, Niagara_B);

	DOREPLIFETIME(AMassUnitBase, Niagara_A_Start_Transform);
	DOREPLIFETIME(AMassUnitBase, Niagara_B_Start_Transform);
	
	DOREPLIFETIME(AMassUnitBase, MeshRotationOffset);
	
	DOREPLIFETIME(AMassUnitBase, IsFlying);
	DOREPLIFETIME(AMassUnitBase, FlyHeight);
}

FVector AMassUnitBase::GetMassActorLocation() const
{
	if (!bUseSkeletalMovement && ISMComponent && !bUseIsmWithActorMovement)
	{
		FTransform Xform;
		ISMComponent->GetInstanceTransform(InstanceIndex, Xform, true);
		return Xform.GetLocation();
	}

	return Super::GetMassActorLocation();
}

bool AMassUnitBase::SetInvisibility(bool NewInvisibility)
{
	
	if (!MassActorBindingComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): FocusEntityTarget failed - TargetUnit is null or has no MassActorBindingComponent."), *GetName());
		return false;
	}
	
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	
	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		// Error already logged in GetMassEntityData
		UE_LOG(LogTemp, Warning, TEXT("!!!NO ENTITY OR MANGER FOUND!!!"));
	
		return false;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): SwitchEntityTagByState failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}
	
	// Reset state timers
	FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle);
	
	if (!CharFrag) return false;

	AUnitBase* Unit = Cast<AUnitBase>(this);
	
	Unit->bIsInvisible = NewInvisibility;
	Unit->bCanBeInvisible = NewInvisibility;
	CharFrag->bIsInvisible = NewInvisibility;
	CharFrag->bCanBeInvisible = NewInvisibility;
	
	return true;
}

bool AMassUnitBase::AddStopMovementTagToEntity()
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
	
	EntityManager->Defer().AddTag<FMassStateStopMovementTag>(EntityHandle);
	
	return true;
}


bool AMassUnitBase::EnableDynamicObstacle(bool Enable)
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

	if (!Enable)
	{
		EntityManager->Defer().AddTag<FMassStateDisableObstacleTag>(EntityHandle);
	}
	else
	{
		EntityManager->Defer().RemoveTag<FMassStateDisableObstacleTag>(EntityHandle);
	}
	
	return true;
}

bool AMassUnitBase::SetUnitAvoidanceEnabled(bool bEnable)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle) || !EntityManager->IsEntityValid(EntityHandle))
		return false;

	auto& Defer = EntityManager->Defer();

	if (bEnable)
	{
		Defer.RemoveTag<FMassDisableAvoidanceTag>(EntityHandle);
	}
	else
	{
		Defer.AddTag<FMassDisableAvoidanceTag>(EntityHandle);
	}

	return true;
}

bool AMassUnitBase::RemoveStopGameplayEffectTagToEntity()
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
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): RemoveTagFromEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}

	
	EntityManager->Defer().RemoveTag<FMassStopGameplayEffectTag>(EntityHandle);
	// Add the tag

	return true;
}


bool AMassUnitBase::AddStopGameplayEffectTagToEntity()
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
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): AddTagToEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}


	EntityManager->Defer().AddTag<FMassStopGameplayEffectTag>(EntityHandle);
	
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
	if (UState != UnitData::Idle) EntityManager->Defer().RemoveTag<FMassStateIdleTag>(EntityHandle);
	if (UState != UnitData::Chase) EntityManager->Defer().RemoveTag<FMassStateChaseTag>(EntityHandle);
	if (UState != UnitData::Attack) EntityManager->Defer().RemoveTag<FMassStateAttackTag>(EntityHandle);
	if (UState != UnitData::Pause) EntityManager->Defer().RemoveTag<FMassStatePauseTag>(EntityHandle);
	//if (UState != UnitData::Dead) EntityManager->Defer().RemoveTag<FMassStateDeadTag>(EntityHandle);
	if (UState != UnitData::Run) EntityManager->Defer().RemoveTag<FMassStateRunTag>(EntityHandle);
	if (UState != UnitData::PatrolRandom) EntityManager->Defer().RemoveTag<FMassStatePatrolRandomTag>(EntityHandle);
	if (UState != UnitData::PatrolIdle) EntityManager->Defer().RemoveTag<FMassStatePatrolIdleTag>(EntityHandle);
	if (UState != UnitData::Casting) EntityManager->Defer().RemoveTag<FMassStateCastingTag>(EntityHandle);
	if (UState != UnitData::IsAttacked) EntityManager->Defer().RemoveTag<FMassStateIsAttackedTag>(EntityHandle);
	if (UState != UnitData::GoToBase) EntityManager->Defer().RemoveTag<FMassStateGoToBaseTag>(EntityHandle);
	if (UState != UnitData::GoToBuild) EntityManager->Defer().RemoveTag<FMassStateGoToBuildTag>(EntityHandle);
	if (UState != UnitData::Build) EntityManager->Defer().RemoveTag<FMassStateBuildTag>(EntityHandle);
	if (UState != UnitData::GoToResourceExtraction) EntityManager->Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(EntityHandle);
	if (UState != UnitData::ResourceExtraction) EntityManager->Defer().RemoveTag<FMassStateResourceExtractionTag>(EntityHandle);

	// Reset state timers
	FMassAIStateFragment* StateFrag = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle);
	StateFrag->StateTimer = 0.f;
	UnitControlTimer = 0.f;

	// Set PlaceholderSignal using UnitStatePlaceholder
	switch (UStatePlaceholder)
	{
	case UnitData::Idle: StateFrag->PlaceholderSignal = UnitSignals::Idle; break;
	//case UnitData::Chase: StateFrag->PlaceholderSignal = UnitSignals::Chase; break;
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
	
	
	// The EntityManager is now handled within the switch
	SetUnitState(UState);
	auto& Defer = EntityManager->Defer();

	switch (UState)
	{
	    case UnitData::Idle:
		    {
			    Defer.AddTag<FMassStateIdleTag>(EntityHandle);
	    		if (StateFrag->CanAttack && StateFrag->IsInitialized) Defer.AddTag<FMassStateDetectTag>(EntityHandle);
		    }
	        break;

	    case UnitData::Chase:
			    Defer.AddTag<FMassStateChaseTag>(EntityHandle);
	        break;

	    case UnitData::Attack:
	        Defer.AddTag<FMassStateAttackTag>(EntityHandle);
	        break;

	    case UnitData::Pause:
	        Defer.AddTag<FMassStatePauseTag>(EntityHandle);
	        break;

	    case UnitData::Dead:
	        Defer.AddTag<FMassStateDeadTag>(EntityHandle);
	        break;

	    case UnitData::Run:
	        Defer.AddTag<FMassStateRunTag>(EntityHandle);
	        break;

	    case UnitData::PatrolRandom:
		    {
			    Defer.AddTag<FMassStatePatrolRandomTag>(EntityHandle);
	    		if (StateFrag->CanAttack && StateFrag->IsInitialized)Defer.AddTag<FMassStateDetectTag>(EntityHandle);
		    }
	        break;

	    case UnitData::PatrolIdle:
		    {
			    Defer.AddTag<FMassStatePatrolIdleTag>(EntityHandle);
	    		if (StateFrag->CanAttack && StateFrag->IsInitialized)Defer.AddTag<FMassStateDetectTag>(EntityHandle);
		    }
	        break;

	    case UnitData::Casting:
	        Defer.AddTag<FMassStateCastingTag>(EntityHandle);
	        break;

	    case UnitData::IsAttacked:
	        Defer.AddTag<FMassStateIsAttackedTag>(EntityHandle);
	        break;

	    case UnitData::GoToBase:
	        Defer.AddTag<FMassStateGoToBaseTag>(EntityHandle);
	        break;

	    case UnitData::GoToBuild:
	        Defer.AddTag<FMassStateGoToBuildTag>(EntityHandle);
	        break;

	    case UnitData::Build:
	        Defer.AddTag<FMassStateBuildTag>(EntityHandle);
	        break;

	    case UnitData::GoToResourceExtraction:
	        Defer.AddTag<FMassStateGoToResourceExtractionTag>(EntityHandle);
	        break;

	    case UnitData::ResourceExtraction:
	        Defer.AddTag<FMassStateResourceExtractionTag>(EntityHandle);
	        break;

	    default:
	        UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): Unknown UnitState."), *GetName());
	        return false; // Or handle error appropriately
	}
		

	if (UMassSignalSubsystem* SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>())
	{

		SignalSubsystem->SignalEntity(UnitSignals::SyncUnitBase, EntityHandle);
	}

	return true;
}


bool AMassUnitBase::UpdateEntityStateOnUnload(const FVector& UnloadLocation)
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
	
				// Update the AI's stored location
				if (FMassAIStateFragment* AiStatePtr = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle))
				{
					AiStatePtr->StoredLocation = UnloadLocation;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("UpdateEntityStateOnUnload: Entity %s does not have an FMassAIStateFragment."), *EntityHandle.DebugGetDescription());
				}

				// Update the movement target
				if (FMassMoveTargetFragment* MoveTarget = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
				{
					MoveTarget->Center = UnloadLocation;
				}
                
				// Allow the entity to move again by removing the stop tag
				EntityManager->Defer().RemoveTag<FMassStateStopMovementTag>(EntityHandle);
			
	return true;
	
}



bool AMassUnitBase::ResetTarget()
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
	if (FMassAITargetFragment* TargetFrag = EntityManager->GetFragmentDataPtr<FMassAITargetFragment>(EntityHandle))
	{
		TargetFrag->TargetEntity.Reset();
		TargetFrag->bHasValidTarget = false;
		return true;
	}

	return false;
}

bool AMassUnitBase::EditUnitDetection(bool HasDetection)
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

	if (HasDetection)
		EntityManager->Defer().AddTag<FMassStateDetectTag>(EntityHandle);
	else
		EntityManager->Defer().RemoveTag<FMassStateDetectTag>(EntityHandle);
	
	return true;
}

bool AMassUnitBase::EditUnitDetectable(bool IsDetectable)
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

	if (!IsDetectable)
		EntityManager->Defer().AddTag<FMassStopUnitDetectionTag>(EntityHandle);
	else
		EntityManager->Defer().RemoveTag<FMassStopUnitDetectionTag>(EntityHandle);
	
	return true;
}

bool AMassUnitBase::IsUnitDetectable()
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
	
	
	return !DoesEntityHaveTag(*EntityManager, EntityHandle, FMassStopUnitDetectionTag::StaticStruct());
}


bool AMassUnitBase::FocusEntityTarget(AUnitBase* TargetUnit)
{
	
	if (!TargetUnit || !TargetUnit->MassActorBindingComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): FocusEntityTarget failed - TargetUnit is null or has no MassActorBindingComponent."), *GetName());
		return false;
	}
	
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

	FMassEntityHandle TargetEntity = TargetUnit->MassActorBindingComponent->GetEntityHandle();
	
	if (!EntityManager->IsEntityValid(TargetEntity))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): FocusEntityTarget failed - Target entity %s is no longer valid."), *GetName(), *TargetEntity.DebugGetDescription());
		return false;
	}
	// Reset state timers
	FMassAITargetFragment* TargetFrag = EntityManager->GetFragmentDataPtr<FMassAITargetFragment>(EntityHandle);
	
	if (!TargetFrag) return false;
	
	TargetFrag->TargetEntity      = TargetEntity;
	TargetFrag->IsFocusedOnTarget = true;

	return true;
}


bool AMassUnitBase::RemoveFocusEntityTarget()
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
	
	FMassAITargetFragment* TargetFrag = EntityManager->GetFragmentDataPtr<FMassAITargetFragment>(EntityHandle);
	
	if (!TargetFrag) return false;
	
	TargetFrag->IsFocusedOnTarget = false;

	return true;
}


bool AMassUnitBase::UpdateEntityHealth(float NewHealth)
{

	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	
	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		return false;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		//UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): SwitchEntityTagByState failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}
	
	
	// Reset state timers
	FMassCombatStatsFragment* CombatStats = EntityManager->GetFragmentDataPtr<FMassCombatStatsFragment>(EntityHandle);
	
	if (!CombatStats) return false;
	
	CombatStats->Health = NewHealth;

	if (CombatStats->Health <= 0.f)
	{
		SwitchEntityTag(FMassStateDeadTag::StaticStruct());
	}

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
	if (TagToAdd != FMassStateIdleTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateIdleTag>(EntityHandle);
	if (TagToAdd != FMassStateChaseTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateChaseTag>(EntityHandle);

	if (TagToAdd != FMassStateAttackTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateAttackTag>(EntityHandle);
	if (TagToAdd != FMassStatePauseTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStatePauseTag>(EntityHandle);
	//if (TagToAdd != FMassStateDeadTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateDeadTag>(EntityHandle);
	if (TagToAdd != FMassStateRunTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateRunTag>(EntityHandle);
	if (TagToAdd != FMassStatePatrolRandomTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStatePatrolRandomTag>(EntityHandle);
	if (TagToAdd != FMassStatePatrolIdleTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStatePatrolIdleTag>(EntityHandle);
	if (TagToAdd != FMassStateCastingTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateCastingTag>(EntityHandle);
	if (TagToAdd != FMassStateIsAttackedTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateIsAttackedTag>(EntityHandle);
	if (TagToAdd != FMassStateGoToBaseTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateGoToBaseTag>(EntityHandle);
	if (TagToAdd != FMassStateGoToBuildTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateGoToBuildTag>(EntityHandle);
	if (TagToAdd != FMassStateBuildTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateBuildTag>(EntityHandle);

	if (TagToAdd != FMassStateGoToResourceExtractionTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(EntityHandle);
	if (TagToAdd != FMassStateResourceExtractionTag::StaticStruct()) EntityManager->Defer().RemoveTag<FMassStateResourceExtractionTag>(EntityHandle);


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
	
		
	if      (TagToAdd == FMassStateIdleTag::StaticStruct())
	{
	    SetUnitState(UnitData::Idle);
	    EntityManager->Defer().AddTag<FMassStateIdleTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateChaseTag::StaticStruct())
	{
	    SetUnitState(UnitData::Chase);
	    EntityManager->Defer().AddTag<FMassStateChaseTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateAttackTag::StaticStruct())
	{
	    SetUnitState(UnitData::Attack);
	    EntityManager->Defer().AddTag<FMassStateAttackTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStatePauseTag::StaticStruct())
	{
	    SetUnitState(UnitData::Pause);
	    EntityManager->Defer().AddTag<FMassStatePauseTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateDeadTag::StaticStruct())
	{
	    SetUnitState(UnitData::Dead);
	    EntityManager->Defer().AddTag<FMassStateDeadTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateRunTag::StaticStruct())
	{
	    SetUnitState(UnitData::Run);
	    EntityManager->Defer().AddTag<FMassStateRunTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStatePatrolRandomTag::StaticStruct())
	{
	    SetUnitState(UnitData::PatrolRandom);
	    EntityManager->Defer().AddTag<FMassStatePatrolRandomTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStatePatrolIdleTag::StaticStruct())
	{
	    SetUnitState(UnitData::PatrolIdle);
	    EntityManager->Defer().AddTag<FMassStatePatrolIdleTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateCastingTag::StaticStruct())
	{
	    SetUnitState(UnitData::Casting);
	    EntityManager->Defer().AddTag<FMassStateCastingTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateIsAttackedTag::StaticStruct())
	{
	    SetUnitState(UnitData::IsAttacked);
	    EntityManager->Defer().AddTag<FMassStateIsAttackedTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateGoToBaseTag::StaticStruct())
	{
	    SetUnitState(UnitData::GoToBase);
	    EntityManager->Defer().AddTag<FMassStateGoToBaseTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateGoToBuildTag::StaticStruct())
	{
	    SetUnitState(UnitData::GoToBuild);
	    EntityManager->Defer().AddTag<FMassStateGoToBuildTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateBuildTag::StaticStruct())
	{
	    SetUnitState(UnitData::Build);
	    EntityManager->Defer().AddTag<FMassStateBuildTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateGoToResourceExtractionTag::StaticStruct())
	{
	    SetUnitState(UnitData::GoToResourceExtraction);
	    EntityManager->Defer().AddTag<FMassStateGoToResourceExtractionTag>(EntityHandle);
	}
	else if (TagToAdd == FMassStateResourceExtractionTag::StaticStruct())
	{
	    SetUnitState(UnitData::ResourceExtraction);
	    EntityManager->Defer().AddTag<FMassStateResourceExtractionTag>(EntityHandle);
	}

	if (IsWorker)
	{
		if (UMassSignalSubsystem* SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>())
		{
			SignalSubsystem->SignalEntity(UnitSignals::UpdateWorkerMovement, EntityHandle);
		}
	}
	return true;
}

bool AMassUnitBase::GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle)
{

	
	OutEntityManager = nullptr;
	OutEntityHandle.Reset();
	
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("AMassUnitBase (%s): Cannot get Mass Entity Data - World is invalid."), *GetName());
		return false;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AMassUnitBase (%s): Cannot get Mass Entity Data - UMassEntitySubsystem is invalid."), *GetName());
		return false;
	}

	if (!MassActorBindingComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AMassUnitBase (%s): Cannot get Mass Entity Data - MassActorBindingComponent is missing."), *GetName());
		return false;
	}

	OutEntityHandle = MassActorBindingComponent->GetEntityHandle();
	if (!OutEntityHandle.IsSet())
	{
		// This might be expected if the entity hasn't been registered yet, so using Warning.
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): Cannot get Mass Entity Data - Entity Handle is not set in Binding Component."), *GetName());
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

void AMassUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop any pending rotation timer
	GetWorldTimerManager().ClearTimer(RotateTimerHandle);

	// Remove our specific instance from the component when the actor is destroyed
	if (InstanceIndex != INDEX_NONE && ISMComponent)
	{
		ISMComponent->RemoveInstance(InstanceIndex);
		InstanceIndex = INDEX_NONE; // Reset the index
	}

	Super::EndPlay(EndPlayReason);
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
	
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->Deactivate();
			MoveComp->SetComponentTickEnabled(false);
		}
	}

	
	if (Niagara_A)
	{
		Niagara_A_Start_Transform = Niagara_A->GetRelativeTransform();
	}

	if (Niagara_B)
	{
		Niagara_B_Start_Transform = Niagara_B->GetRelativeTransform();
	}
	
	// Add the instance when the actor is ready and in the world
	if (!bUseSkeletalMovement && ISMComponent && ISMComponent->GetStaticMesh())
	{

		const FTransform LocalIdentityTransform = FTransform::Identity;
		
		if (InstanceIndex == INDEX_NONE)
		{
			// Add a new instance at the component's local origin.
			InstanceIndex = ISMComponent->AddInstance(LocalIdentityTransform, false);
			MeshRotationOffset = ISMComponent->GetRelativeRotation().Quaternion();
		}
		else
		{
			// Update the existing instance to be at the component's local origin.
			ISMComponent->UpdateInstanceTransform(InstanceIndex, LocalIdentityTransform, false, true, true);
			MeshRotationOffset = ISMComponent->GetRelativeRotation().Quaternion();
		}
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


bool AMassUnitBase::SetTranslationLocation(FVector NewLocation)
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
	Current.SetTranslation(NewLocation);

	return true;
}

bool AMassUnitBase::UpdatePredictionFragment(const FVector& NewLocation, float DesiredSpeed)
{
	if (GetNetMode() != NM_Client ) return false;
	
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle   EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		return false;
	}
	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): UpdateMovementAndPredictionAtLocation failed – entity %s invalid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}

	// Update client prediction fragment
	if (FMassClientPredictionFragment* Pred = EntityManager->GetFragmentDataPtr<FMassClientPredictionFragment>(EntityHandle))
	{
		Pred->Location = NewLocation;
		Pred->PredDesiredSpeed = DesiredSpeed;
		Pred->bHasData = true;
	}

	return true;
}

void AMassUnitBase::Multicast_UpdateISMInstanceTransform_Implementation(int32 InstIndex,
	const FTransform& NewTransform)
{
	if (ISMComponent && ISMComponent->IsValidInstance(InstIndex))
	{
		ISMComponent->UpdateInstanceTransform(InstIndex, NewTransform, true, true, false);
	}
	
	if (Niagara_A && !Niagara_A->bHiddenInGame)
	{
		// Get the local offset vector (e.g., FVector(-750, 0, 0)) from the start transform
		const FVector LocalOffset = Niagara_A_Start_Transform.GetLocation();

		// Rotate this local offset by the new instance's world rotation
		const FVector WorldOffset = NewTransform.GetRotation().RotateVector(LocalOffset);
    
		// Add the correctly rotated offset to the instance's location
		const FVector FinalWorldLocation = NewTransform.GetLocation() + WorldOffset;

		// Combine the instance's rotation with the component's relative rotation
		const FQuat FinalWorldRotation = NewTransform.GetRotation() * Niagara_A_Start_Transform.GetRotation();
       
		// Create the final transform
		const FTransform FinalWorldTransform(FinalWorldRotation, FinalWorldLocation, NewTransform.GetScale3D());

		// Set the final transform on the Niagara component
		Niagara_A->SetWorldTransform(FinalWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Manually move Niagara_B to the new location
	if (Niagara_B && !Niagara_B->bHiddenInGame)
	{
		// Get the local offset vector (e.g., FVector(-750, 0, 0)) from the start transform
		const FVector LocalOffset = Niagara_B_Start_Transform.GetLocation();

		// Rotate this local offset by the new instance's world rotation
		const FVector WorldOffset = NewTransform.GetRotation().RotateVector(LocalOffset);
    
		// Add the correctly rotated offset to the instance's location
		const FVector FinalWorldLocation = NewTransform.GetLocation() + WorldOffset;

		// Combine the instance's rotation with the component's relative rotation
		const FQuat FinalWorldRotation = NewTransform.GetRotation() * Niagara_B_Start_Transform.GetRotation();
       
		// Create the final transform
		const FTransform FinalWorldTransform(FinalWorldRotation, FinalWorldLocation, NewTransform.GetScale3D());

		// Set the final transform on the Niagara component
		Niagara_B->SetWorldTransform(FinalWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	
	if (SelectionIcon && !SelectionIcon->bHiddenInGame)
	{
		// Setze die Welt-Position des Decals auf die neue Welt-Position der Instanz.
		// Die Rotation des Decals muss nicht geändert werden, da es immer nach unten projiziert.
		FVector NewLocation = NewTransform.GetLocation();
	
		if (IsFlying)
			NewLocation.Z = NewLocation.Z-FlyHeight;
		
		SelectionIcon->SetWorldLocation(NewLocation);
	}
}


void AMassUnitBase::StartAcceleratingTowardsDestination(const FVector& NewDestination, float NewAccelerationRate, float NewRequiredDistanceToStart)
{
	// Don't initiate a charge if the unit is dead.
	if (GetUnitState() == UnitData::Dead)
	{
		return;
	}
    
	// Compute the current distance to the destination.
	const FVector CurrentLocation = GetActorLocation();
	const float DistanceToTarget = FVector::Dist(CurrentLocation, NewDestination);
    
	// If already close enough, abort the charge.
	if (DistanceToTarget <= NewRequiredDistanceToStart)
	{
		return;
	}
	
	Attributes->SetRunSpeed(Attributes->GetRunSpeed()*10);
}

void AMassUnitBase::StartCharge(const FVector& NewDestination, float ChargeSpeed, float ChargeDuration)
{
    if (GetUnitState() == UnitData::Dead)
    {
        return;
    }

    FMassEntityManager* EntityManager;
    FMassEntityHandle EntityHandle;

    if (!GetMassEntityData(EntityManager, EntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): StartCharge failed - could not get Mass entity data."), *GetName());
        return;
    }

    if (!EntityManager->IsEntityValid(EntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): StartCharge failed - entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
        return;
    }

    // --- 1. Get/Add FMassChargeTimerFragment ---
    FMassChargeTimerFragment* ChargeTimer = EntityManager->GetFragmentDataPtr<FMassChargeTimerFragment>(EntityHandle);
    if (!ChargeTimer)
    {
    	return;
    }
	
    // --- 2. Update FMassMoveTargetFragment ---
    FMassMoveTargetFragment* MoveTarget = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle);
    if (!MoveTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): StartCharge failed - FMassMoveTargetFragment not found on entity %s."), *GetName(), *EntityHandle.DebugGetDescription());
        EntityManager->RemoveFragmentFromEntity(EntityHandle, FMassChargeTimerFragment::StaticStruct()); // Clean up
        return;
    }
	
	UpdateMoveTarget(*MoveTarget, NewDestination, ChargeSpeed, GetWorld());
	
    // --- 3. Set Charge Timer and Tag ---
    ChargeTimer->ChargeEndTime = ChargeDuration;
	ChargeTimer->OriginalDesiredSpeed = Attributes->GetRunSpeed();
	ChargeTimer->bOriginalSpeedSet = true;


	FMassAIStateFragment* StateFrag = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle);
	if (!StateFrag)
	{
		UE_LOG(LogTemp, Warning, TEXT("NO STATE FRAG FOUND"));
		return;
	}

	FMassCombatStatsFragment* StatsFrag = EntityManager->GetFragmentDataPtr<FMassCombatStatsFragment>(EntityHandle);
	if (!StatsFrag)
	{
		UE_LOG(LogTemp, Warning, TEXT("NO StatsFrag FOUND"));
		return;
	}

	StateFrag->StateTimer = 0.f;
	StatsFrag->RunSpeed = ChargeSpeed;

    //EntityManager->AddTagToEntity(EntityHandle, FMassStateChargingTag::StaticStruct());
	EntityManager->Defer().AddTag<FMassStateChargingTag>(EntityHandle);
}

bool AMassUnitBase::StopMassMovement()
{
	// Only meaningful on authority/server
	if (GetNetMode() == NM_Client)
	{
		return false;
	}

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle   EntityHandle;
	if (!GetMassEntityData(EntityManager, EntityHandle) || EntityManager == nullptr || !EntityManager->IsEntityValid(EntityHandle))
	{
		return false;
	}

	if (FMassMoveTargetFragment* MoveTarget = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
	{
		StopMovement(*MoveTarget, GetWorld());
		return true;
	}

	return false;
}

void AMassUnitBase::MulticastTransformSync_Implementation(const FVector& Location)
{
	//SetActorLocation(Location);
	SetTranslationLocation(Location);
	UpdatePredictionFragment(Location, Attributes->GetBaseRunSpeed());
	StopMassMovement();
	SwitchEntityTagByState(UnitData::Idle, UnitStatePlaceholder);
}

void AMassUnitBase::MulticastRotateISMLinear_Implementation(const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent)
{
	// Assign incoming parameters to the global variables so all machines share identical tween settings
	RotateDuration = InRotateDuration;
	// Clamp ease exponent to a small positive value to avoid degenerate alpha mapping
	RotationEaseExponent = FMath::Max(InRotationEaseExponent, 0.001f);

	// Start a smooth rotation on both server and clients using a local timer per machine.
	if (!ISMComponent && !GetMesh())
	{
		return;
	}

	// Desired target (component/local or world for ISM instance). Apply mesh offset so visuals align with logical forward.
	const FQuat DesiredLocal = (NewRotation.Quaternion() * MeshRotationOffset).GetNormalized();

	// If duration is not positive, snap immediately.
	if (RotateDuration <= 0.f)
	{
		ApplyLocalVisualRotation(DesiredLocal);
		return;
	}

	// Initialize interpolation state and start/update the timer on this machine.
	GetWorldTimerManager().ClearTimer(RotateTimerHandle);
	RotateStart = GetCurrentLocalVisualRotation();
	RotateTarget = DesiredLocal;
	RotateElapsed = 0.f;

	// Use a small, non-zero rate so the timer actually loops on all platforms/versions.
	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);

	// Kick the first step immediately for responsiveness
	RotateISM_Step();

	GetWorldTimerManager().SetTimer(RotateTimerHandle, this, &AMassUnitBase::RotateISM_Step, TimerRate, true);
}

FQuat AMassUnitBase::GetCurrentLocalVisualRotation() const
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ true);
			return InstanceXf.GetRotation();
		}
		// Fallback to component's relative rotation if no valid instance
		return ISMComponent->GetRelativeRotation().Quaternion();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeRotation().Quaternion();
	}

	return GetActorQuat();
}

void AMassUnitBase::ApplyLocalVisualRotation(const FQuat& NewLocalRotation)
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ true);
			InstanceXf.SetRotation(NewLocalRotation);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		// Fallback: rotate the whole ISM component
		ISMComponent->SetRelativeRotation(NewLocalRotation.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeRotation(NewLocalRotation.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AMassUnitBase::RotateISM_Step()
{
	// If we lost visual components, bail out and stop the timer
	if (!ISMComponent && !GetMesh())
	{
		GetWorldTimerManager().ClearTimer(RotateTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	RotateElapsed += Dt;

	float Alpha = (RotateDuration > 0.f) ? FMath::Clamp(RotateElapsed / RotateDuration, 0.f, 1.f) : 1.f;
	if (!FMath::IsNearlyEqual(RotationEaseExponent, 1.f))
	{
		Alpha = FMath::Pow(Alpha, RotationEaseExponent);
	}

	const FQuat NewLocal = FQuat::Slerp(RotateStart, RotateTarget, Alpha).GetNormalized();
	ApplyLocalVisualRotation(NewLocal);

	if (Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(RotateTimerHandle);
	}
}