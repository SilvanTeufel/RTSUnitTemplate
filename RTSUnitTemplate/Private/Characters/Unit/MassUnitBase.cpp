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
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"

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

bool AMassUnitBase::SetNavManipulationEnabled(bool bEnable)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle) || !EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): SetNavManipulationEnabled failed - invalid entity or manager."), *GetName());
		return false;
	}

	auto& Defer = EntityManager->Defer();

	if (bEnable)
	{
		// Enable nav manipulation: remove the disabling tag if present
		Defer.RemoveTag<FMassStateDisableNavManipulationTag>(EntityHandle);
	}
	else
	{
		// Disable nav manipulation: add the disabling tag
		Defer.AddTag<FMassStateDisableNavManipulationTag>(EntityHandle);
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
	// Repair-specific
	if (UState != UnitData::GoToRepair) EntityManager->Defer().RemoveTag<FMassStateGoToRepairTag>(EntityHandle);
	if (UState != UnitData::Repair) EntityManager->Defer().RemoveTag<FMassStateRepairTag>(EntityHandle);

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
	case UnitData::GoToRepair: StateFrag->PlaceholderSignal = UnitSignals::GoToRepair; break;
	case UnitData::Repair: StateFrag->PlaceholderSignal = UnitSignals::Repair; break;
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
	
	    case UnitData::GoToRepair:
	        Defer.AddTag<FMassStateGoToRepairTag>(EntityHandle);
	        break;
	
	    case UnitData::Repair:
	        Defer.AddTag<FMassStateRepairTag>(EntityHandle);
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
				if (FMassMoveTargetFragment* MoveTargetFrag = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
				{
					MoveTargetFrag->Center = UnloadLocation;
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
	TargetFrag->TargetEntity.Reset();
	
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
		// This is expected during early initialization or while Mass is scheduling creation.
		const UWorld* W = GetWorld();
		const float Now = W ? W->GetTimeSeconds() : 0.f;
		const bool bSettingUp = MassActorBindingComponent->bNeedsMassUnitSetup || MassActorBindingComponent->bNeedsMassBuildingSetup;
		if (bSettingUp || Now < 2.2f)
		{
			UE_LOG(LogTemp, Verbose, TEXT("AMassUnitBase (%s): Mass Entity Handle not set yet (initializing)."), *GetName());
			return false;
		}
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
	// Stop any pending rotation/movement timers
	GetWorldTimerManager().ClearTimer(RotateTimerHandle);
	GetWorldTimerManager().ClearTimer(StaticMeshRotateTimerHandle);
	GetWorldTimerManager().ClearTimer(StaticMeshYawFollowTimerHandle);
	ActiveStaticMeshTweens.Empty();
	ActiveYawFollows.Empty();

	GetWorldTimerManager().ClearTimer(MoveTimerHandle);
	GetWorldTimerManager().ClearTimer(StaticMeshMoveTimerHandle);
	ActiveStaticMeshMoveTweens.Empty();

	// Stop any pending scaling timers
	GetWorldTimerManager().ClearTimer(ScaleTimerHandle);
	GetWorldTimerManager().ClearTimer(StaticMeshScaleTimerHandle);
	GetWorldTimerManager().ClearTimer(PulsateScaleTimerHandle);
	bPulsateScaleEnabled = false;
	ActiveStaticMeshScaleTweens.Empty();

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
			MeshRotationOffset = ISMComponent->GetRelativeRotation().Quaternion().GetNormalized();
		}
		else
		{
			// Update the existing instance to be at the component's local origin.
			ISMComponent->UpdateInstanceTransform(InstanceIndex, LocalIdentityTransform, false, true, true);
			MeshRotationOffset = ISMComponent->GetRelativeRotation().Quaternion().GetNormalized();
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
    FMassMoveTargetFragment* MoveTargetFrag = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle);
    if (!MoveTargetFrag)
    {
        UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): StartCharge failed - FMassMoveTargetFragment not found on entity %s."), *GetName(), *EntityHandle.DebugGetDescription());
        EntityManager->RemoveFragmentFromEntity(EntityHandle, FMassChargeTimerFragment::StaticStruct()); // Clean up
        return;
    }
	
	UpdateMoveTarget(*MoveTargetFrag, NewDestination, ChargeSpeed, GetWorld());
	
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

	if (FMassMoveTargetFragment* MoveTargetFrag = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
	{
		StopMovement(*MoveTargetFrag, GetWorld());
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

	// Desired target (component/local or world for ISM instance). Interpret NewRotation as the exact end orientation for the visual.
	// Do NOT bake in MeshRotationOffset here, otherwise yaw-only inputs may introduce pitch/roll during the tween.
	const FQuat DesiredLocal = NewRotation.Quaternion().GetNormalized();

	// If duration is not positive, snap immediately.
	if (RotateDuration <= 0.f)
	{
		ApplyLocalVisualRotation(DesiredLocal);
		return;
	}

	// Initialize interpolation state and start/update the timer on this machine.
	GetWorldTimerManager().ClearTimer(RotateTimerHandle);
	RotateStart = GetCurrentLocalVisualRotation().GetNormalized();
	RotateTarget = DesiredLocal;
	// Ensure shortest-arc interpolation by flipping target if needed (q and -q represent same rotation)
	if ((RotateStart | RotateTarget) < 0.f)
	{
		RotateTarget = (-RotateTarget);
	}
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
			// Read instance LOCAL transform (not world) to avoid pulling in actor/world rotation
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			return InstanceXf.GetRotation().GetNormalized();
		}
		// Fallback to component's relative rotation if no valid instance
		return ISMComponent->GetRelativeRotation().Quaternion().GetNormalized();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeRotation().Quaternion().GetNormalized();
	}

	return GetActorQuat().GetNormalized();
}

void AMassUnitBase::ApplyLocalVisualRotation(const FQuat& NewLocalRotation)
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		const FQuat Visual = NewLocalRotation.GetNormalized();
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			// Write instance LOCAL transform so we don't blend in world/actor rotation
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			InstanceXf.SetRotation(Visual);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		// Fallback: rotate the whole ISM component
		ISMComponent->SetRelativeRotation(Visual.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
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
		// Snap exactly to the desired final orientation to avoid accumulated numeric error
		ApplyLocalVisualRotation(RotateTarget);
		GetWorldTimerManager().ClearTimer(RotateTimerHandle);
	}
}

void AMassUnitBase::MulticastRotateActorLinear_Implementation(UStaticMeshComponent* MeshToRotate, const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent)
{
	if (!MeshToRotate)
	{
		return;
	}

	// Instant snap if duration <= 0
	if (InRotateDuration <= 0.f)
	{
		MeshToRotate->SetRelativeRotation(NewRotation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Setup or update tween for this component
 AMassUnitBase::FStaticMeshRotateTween& Tween = ActiveStaticMeshTweens.FindOrAdd(MeshToRotate);
	Tween.Duration = InRotateDuration;
	Tween.Elapsed = 0.f;
	Tween.EaseExp = FMath::Max(InRotationEaseExponent, 0.001f);
	Tween.Start = MeshToRotate->GetRelativeRotation().Quaternion().GetNormalized();
	Tween.Target = NewRotation.Quaternion().GetNormalized();
	// Enforce shortest-arc interpolation: if start and target are on opposite hemispheres, flip target
	if ((Tween.Start | Tween.Target) < 0.f)
	{
		Tween.Target = (-Tween.Target);
	}

	// Ensure timer is running with a safe non-zero rate
	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);

	// Kick an immediate step for responsiveness
	StaticMeshRotations_Step();

	if (!GetWorldTimerManager().IsTimerActive(StaticMeshRotateTimerHandle))
	{
		GetWorldTimerManager().SetTimer(StaticMeshRotateTimerHandle, this, &AMassUnitBase::StaticMeshRotations_Step, TimerRate, true);
	}
}

void AMassUnitBase::StaticMeshRotations_Step()
{
	if (ActiveStaticMeshTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshRotateTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	TArray<TWeakObjectPtr<UStaticMeshComponent>> ToRemove;
	for (auto& Pair : ActiveStaticMeshTweens)
	{
		TWeakObjectPtr<UStaticMeshComponent> WeakComp = Pair.Key;
		FStaticMeshRotateTween& Tween = Pair.Value;

		UStaticMeshComponent* Comp = WeakComp.Get();
		if (!Comp)
		{
			ToRemove.Add(WeakComp);
			continue;
		}

		Tween.Elapsed += Dt;
		float Alpha = (Tween.Duration > 0.f) ? FMath::Clamp(Tween.Elapsed / Tween.Duration, 0.f, 1.f) : 1.f;
		if (!FMath::IsNearlyEqual(Tween.EaseExp, 1.f))
		{
			Alpha = FMath::Pow(Alpha, Tween.EaseExp);
		}

		const FQuat NewLocal = FQuat::Slerp(Tween.Start, Tween.Target, Alpha).GetNormalized();
		Comp->SetRelativeRotation(NewLocal.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

		if (Alpha >= 1.f)
		{
			// Snap to exact final target orientation to avoid drift
			Comp->SetRelativeRotation(Tween.Target.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
			ToRemove.Add(WeakComp);
		}
	}

	for (const auto& Key : ToRemove)
	{
		ActiveStaticMeshTweens.Remove(Key);
	}

	if (ActiveStaticMeshTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshRotateTimerHandle);
	}
}

void AMassUnitBase::MulticastRotateActorYawToChase_Implementation(UStaticMeshComponent* MeshToRotate, float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees)
{
	if (!MeshToRotate)
	{
		return;
	}

	if (bEnable)
	{
		FYawFollowData& Data = ActiveYawFollows.FindOrAdd(MeshToRotate);
		Data.Duration = InRotateDuration;
		Data.EaseExp = FMath::Max(InRotationEaseExponent, 0.001f);
		Data.OffsetDegrees = YawOffsetDegrees;

		// Start/update the follow timer
		const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
		const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);

		// Kick immediate step then ensure looping
		StaticMeshYawFollow_Step();
		if (!GetWorldTimerManager().IsTimerActive(StaticMeshYawFollowTimerHandle))
		{
			GetWorldTimerManager().SetTimer(StaticMeshYawFollowTimerHandle, this, &AMassUnitBase::StaticMeshYawFollow_Step, TimerRate, true);
		}
	}
	else
	{
		ActiveYawFollows.Remove(MeshToRotate);
		if (ActiveYawFollows.Num() == 0)
		{
			GetWorldTimerManager().ClearTimer(StaticMeshYawFollowTimerHandle);
		}
	}
}

void AMassUnitBase::StaticMeshYawFollow_Step()
{
	if (ActiveYawFollows.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshYawFollowTimerHandle);
		return;
	}

	TArray<TWeakObjectPtr<UStaticMeshComponent>> ToRemove;
	for (auto& Pair : ActiveYawFollows)
	{
		TWeakObjectPtr<UStaticMeshComponent> WeakComp = Pair.Key;
		FYawFollowData& Data = Pair.Value;

		UStaticMeshComponent* Comp = WeakComp.Get();
		if (!Comp)
		{
			ToRemove.Add(WeakComp);
			continue;
		}

		// Use UnitToChase from AUnitBase if available
		AUnitBase* ThisUnit = Cast<AUnitBase>(this);
		AUnitBase* TargetUnit = ThisUnit ? ThisUnit->UnitToChase : nullptr;
		if (!IsValid(TargetUnit))
		{
			// No target; skip but keep following state for when a target appears
			continue;
		}

		const FVector CompWorldLoc = Comp->GetComponentLocation();
		FVector TargetLoc = TargetUnit->GetActorLocation();

		// 2D direction on the XY plane
		FVector ToTarget = TargetLoc - CompWorldLoc;
		ToTarget.Z = 0.f;
		if (ToTarget.IsNearlyZero(1e-3f))
		{
			continue;
		}

		const FRotator FacingRot = FRotationMatrix::MakeFromX(ToTarget.GetSafeNormal()).Rotator();
		float DesiredWorldYaw = FacingRot.Yaw + Data.OffsetDegrees;
		DesiredWorldYaw = FRotator::NormalizeAxis(DesiredWorldYaw);

		// Convert desired world yaw to relative yaw under parent if any
		float ParentYaw = 0.f;
		if (const USceneComponent* Parent = Comp->GetAttachParent())
		{
			ParentYaw = Parent->GetComponentRotation().Yaw;
		}
		float DesiredRelativeYaw = FRotator::NormalizeAxis(DesiredWorldYaw - ParentYaw);

		FRotator NewRelative = Comp->GetRelativeRotation();
		NewRelative.Yaw = DesiredRelativeYaw;

		// Drive/refresh the tween for this component toward the newly computed yaw-only rotation
		FStaticMeshRotateTween& Tween = ActiveStaticMeshTweens.FindOrAdd(Comp);
		Tween.Duration = Data.Duration;
		Tween.EaseExp = FMath::Max(Data.EaseExp, 0.001f);
		Tween.Elapsed = 0.f;
		Tween.Start = Comp->GetRelativeRotation().Quaternion().GetNormalized();
		Tween.Target = NewRelative.Quaternion().GetNormalized();
		if ((Tween.Start | Tween.Target) < 0.f)
		{
			Tween.Target = (-Tween.Target);
		}
	}

	// Ensure rotation tween timer is running
	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float RotateTimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	StaticMeshRotations_Step();
	if (!GetWorldTimerManager().IsTimerActive(StaticMeshRotateTimerHandle))
	{
		GetWorldTimerManager().SetTimer(StaticMeshRotateTimerHandle, this, &AMassUnitBase::StaticMeshRotations_Step, RotateTimerRate, true);
	}

	for (const auto& Key : ToRemove)
	{
		ActiveYawFollows.Remove(Key);
	}

	if (ActiveYawFollows.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshYawFollowTimerHandle);
	}
}

void AMassUnitBase::MulticastMoveISMLinear_Implementation(const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent)
{
	// Cache parameters
	MoveDuration = InMoveDuration;
	MoveEaseExponent = FMath::Max(InMoveEaseExponent, 0.001f);

	if (!ISMComponent && !GetMesh())
	{
		return;
	}

	// Snap if no duration
	if (MoveDuration <= 0.f)
	{
		ApplyLocalVisualLocation(NewLocation);
		return;
	}

	// Initialize interpolation state
	GetWorldTimerManager().ClearTimer(MoveTimerHandle);
	MoveStart = GetCurrentLocalVisualLocation();
	MoveTarget = NewLocation;
	MoveElapsed = 0.f;

	// Immediate step then start timer
	MoveISM_Step();

	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	GetWorldTimerManager().SetTimer(MoveTimerHandle, this, &AMassUnitBase::MoveISM_Step, TimerRate, true);
}

FVector AMassUnitBase::GetCurrentLocalVisualLocation() const
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			return InstanceXf.GetLocation();
		}
		return ISMComponent->GetRelativeLocation();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeLocation();
	}

	return GetActorLocation();
}

void AMassUnitBase::ApplyLocalVisualLocation(const FVector& NewLocalLocation)
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			InstanceXf.SetLocation(NewLocalLocation);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		ISMComponent->SetRelativeLocation(NewLocalLocation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeLocation(NewLocalLocation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AMassUnitBase::MoveISM_Step()
{
	if (!ISMComponent && !GetMesh())
	{
		GetWorldTimerManager().ClearTimer(MoveTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	MoveElapsed += Dt;

	float Alpha = (MoveDuration > 0.f) ? FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f) : 1.f;
	if (!FMath::IsNearlyEqual(MoveEaseExponent, 1.f))
	{
		Alpha = FMath::Pow(Alpha, MoveEaseExponent);
	}

	const FVector NewLocal = FMath::Lerp(MoveStart, MoveTarget, Alpha);
	ApplyLocalVisualLocation(NewLocal);

	if (Alpha >= 1.f)
	{
		ApplyLocalVisualLocation(MoveTarget);
		GetWorldTimerManager().ClearTimer(MoveTimerHandle);
	}
}

void AMassUnitBase::MulticastMoveActorLinear_Implementation(UStaticMeshComponent* MeshToMove, const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent)
{
	if (!MeshToMove)
	{
		return;
	}

	if (InMoveDuration <= 0.f)
	{
		MeshToMove->SetRelativeLocation(NewLocation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	AMassUnitBase::FStaticMeshMoveTween& Tween = ActiveStaticMeshMoveTweens.FindOrAdd(MeshToMove);
	Tween.Duration = InMoveDuration;
	Tween.Elapsed = 0.f;
	Tween.EaseExp = FMath::Max(InMoveEaseExponent, 0.001f);
	Tween.Start = MeshToMove->GetRelativeLocation();
	Tween.Target = NewLocation;

	// Immediate step and ensure timer running
	StaticMeshMoves_Step();

	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	if (!GetWorldTimerManager().IsTimerActive(StaticMeshMoveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(StaticMeshMoveTimerHandle, this, &AMassUnitBase::StaticMeshMoves_Step, TimerRate, true);
	}
}

void AMassUnitBase::StaticMeshMoves_Step()
{
	if (ActiveStaticMeshMoveTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshMoveTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	TArray<TWeakObjectPtr<UStaticMeshComponent>> ToRemove;
	for (auto& Pair : ActiveStaticMeshMoveTweens)
	{
		TWeakObjectPtr<UStaticMeshComponent> WeakComp = Pair.Key;
		FStaticMeshMoveTween& Tween = Pair.Value;

		UStaticMeshComponent* Comp = WeakComp.Get();
		if (!Comp)
		{
			ToRemove.Add(WeakComp);
			continue;
		}

		Tween.Elapsed += Dt;
		float Alpha = (Tween.Duration > 0.f) ? FMath::Clamp(Tween.Elapsed / Tween.Duration, 0.f, 1.f) : 1.f;
		if (!FMath::IsNearlyEqual(Tween.EaseExp, 1.f))
		{
			Alpha = FMath::Pow(Alpha, Tween.EaseExp);
		}

		const FVector NewLocal = FMath::Lerp(Tween.Start, Tween.Target, Alpha);
		Comp->SetRelativeLocation(NewLocal, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

		if (Alpha >= 1.f)
		{
			Comp->SetRelativeLocation(Tween.Target, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
			ToRemove.Add(WeakComp);
		}
	}

	for (const auto& Key : ToRemove)
	{
		ActiveStaticMeshMoveTweens.Remove(Key);
	}

	if (ActiveStaticMeshMoveTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshMoveTimerHandle);
	}
}

void AMassUnitBase::MulticastScaleISMLinear_Implementation(const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent)
{
	// Cache parameters
	ScaleDuration = InScaleDuration;
	ScaleEaseExponent = FMath::Max(InScaleEaseExponent, 0.001f);

	if (!ISMComponent && !GetMesh())
	{
		return;
	}

	// Snap if no duration
	if (ScaleDuration <= 0.f)
	{
		ApplyLocalVisualScale(NewScale);
		return;
	}

	// Initialize interpolation state
	GetWorldTimerManager().ClearTimer(ScaleTimerHandle);
	ScaleStart = GetCurrentLocalVisualScale();
	ScaleTarget = NewScale;
	ScaleElapsed = 0.f;

	// Immediate step then start timer
	ScaleISM_Step();

	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	GetWorldTimerManager().SetTimer(ScaleTimerHandle, this, &AMassUnitBase::ScaleISM_Step, TimerRate, true);
}

FVector AMassUnitBase::GetCurrentLocalVisualScale() const
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			return InstanceXf.GetScale3D();
		}
		return ISMComponent->GetRelativeScale3D();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeScale3D();
	}

	return GetActorScale3D();
}

void AMassUnitBase::ApplyLocalVisualScale(const FVector& NewLocalScale)
{
	if (!bUseSkeletalMovement && ISMComponent)
	{
		if (ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			InstanceXf.SetScale3D(NewLocalScale);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		ISMComponent->SetRelativeScale3D(NewLocalScale);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeScale3D(NewLocalScale);
	}
}

void AMassUnitBase::ScaleISM_Step()
{
	if (!ISMComponent && !GetMesh())
	{
		GetWorldTimerManager().ClearTimer(ScaleTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	ScaleElapsed += Dt;

	float Alpha = (ScaleDuration > 0.f) ? FMath::Clamp(ScaleElapsed / ScaleDuration, 0.f, 1.f) : 1.f;
	if (!FMath::IsNearlyEqual(ScaleEaseExponent, 1.f))
	{
		Alpha = FMath::Pow(Alpha, ScaleEaseExponent);
	}

	const FVector NewLocal = FMath::Lerp(ScaleStart, ScaleTarget, Alpha);
	ApplyLocalVisualScale(NewLocal);

	if (Alpha >= 1.f)
	{
		ApplyLocalVisualScale(ScaleTarget);
		GetWorldTimerManager().ClearTimer(ScaleTimerHandle);
	}
}

void AMassUnitBase::MulticastScaleActorLinear_Implementation(UStaticMeshComponent* MeshToScale, const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent)
{
	if (!MeshToScale)
	{
		return;
	}

	if (InScaleDuration <= 0.f)
	{
		MeshToScale->SetRelativeScale3D(NewScale);
		return;
	}

	AMassUnitBase::FStaticMeshScaleTween& Tween = ActiveStaticMeshScaleTweens.FindOrAdd(MeshToScale);
	Tween.Duration = InScaleDuration;
	Tween.Elapsed = 0.f;
	Tween.EaseExp = FMath::Max(InScaleEaseExponent, 0.001f);
	Tween.Start = MeshToScale->GetRelativeScale3D();
	Tween.Target = NewScale;

	// Immediate step and ensure timer running
	StaticMeshScales_Step();

	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	if (!GetWorldTimerManager().IsTimerActive(StaticMeshScaleTimerHandle))
	{
		GetWorldTimerManager().SetTimer(StaticMeshScaleTimerHandle, this, &AMassUnitBase::StaticMeshScales_Step, TimerRate, true);
	}
}

void AMassUnitBase::StaticMeshScales_Step()
{
	if (ActiveStaticMeshScaleTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshScaleTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	TArray<TWeakObjectPtr<UStaticMeshComponent>> ToRemove;
	for (auto& Pair : ActiveStaticMeshScaleTweens)
	{
		TWeakObjectPtr<UStaticMeshComponent> WeakComp = Pair.Key;
		FStaticMeshScaleTween& Tween = Pair.Value;

		UStaticMeshComponent* Comp = WeakComp.Get();
		if (!Comp)
		{
			ToRemove.Add(WeakComp);
			continue;
		}

		Tween.Elapsed += Dt;
		float Alpha = (Tween.Duration > 0.f) ? FMath::Clamp(Tween.Elapsed / Tween.Duration, 0.f, 1.f) : 1.f;
		if (!FMath::IsNearlyEqual(Tween.EaseExp, 1.f))
		{
			Alpha = FMath::Pow(Alpha, Tween.EaseExp);
		}

		const FVector NewLocal = FMath::Lerp(Tween.Start, Tween.Target, Alpha);
		Comp->SetRelativeScale3D(NewLocal);

		if (Alpha >= 1.f)
		{
			Comp->SetRelativeScale3D(Tween.Target);
			ToRemove.Add(WeakComp);
		}
	}

	for (const auto& Key : ToRemove)
	{
		ActiveStaticMeshScaleTweens.Remove(Key);
	}

	if (ActiveStaticMeshScaleTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(StaticMeshScaleTimerHandle);
	}
}


void AMassUnitBase::MulticastPulsateISMScale_Implementation(const FVector& InMinScale, const FVector& InMaxScale, float TimeMinToMax, bool bEnable)
{
	// Only proceed if we have a visual to manipulate
	if (!ISMComponent && !GetMesh())
	{
		return;
	}

	// Disable pulsating on request
	if (!bEnable)
	{
		bPulsateScaleEnabled = false;
		GetWorldTimerManager().ClearTimer(PulsateScaleTimerHandle);
		return;
	}

	// Enable and configure pulsation
	bPulsateScaleEnabled = true;
	PulsateMinScale = InMinScale;
	PulsateMaxScale = InMaxScale;
	PulsateHalfPeriod = FMath::Max(TimeMinToMax, 0.0001f); // avoid division by zero
	PulsateElapsed = 0.f;

	// Ensure one-shot scale tween is not fighting with the pulsation
	GetWorldTimerManager().ClearTimer(ScaleTimerHandle);

	// Immediate step for responsiveness
	PulsateISMScale_Step();

	// Start/update timer with a safe rate
	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	GetWorldTimerManager().SetTimer(PulsateScaleTimerHandle, this, &AMassUnitBase::PulsateISMScale_Step, TimerRate, true);
}

void AMassUnitBase::PulsateISMScale_Step()
{
	if (!bPulsateScaleEnabled)
	{
		GetWorldTimerManager().ClearTimer(PulsateScaleTimerHandle);
		return;
	}

	if (!ISMComponent && !GetMesh())
	{
		bPulsateScaleEnabled = false;
		GetWorldTimerManager().ClearTimer(PulsateScaleTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	PulsateElapsed += Dt;

	// Compute a ping-pong alpha in [0,1] with a half-period of PulsateHalfPeriod
	const float Cycle = (PulsateElapsed / PulsateHalfPeriod);
	float Mod = FMath::Fmod(Cycle, 2.f);
	if (Mod < 0.f) Mod += 2.f;
	const float Alpha = (Mod <= 1.f) ? Mod : (2.f - Mod); // 0->1 then 1->0

	const FVector NewScale = FMath::Lerp(PulsateMinScale, PulsateMaxScale, Alpha);
	ApplyLocalVisualScale(NewScale);
}


// Static helper to apply/clear follow target for a unit at the Mass level.
void AMassUnitBase::ApplyFollowTargetForUnit(AUnitBase* ThisUnit, AUnitBase* NewFollowTarget)
{
	if (!ThisUnit)
	{
		return;
	}

	// Assign follow on the actor side and reset cached data so follow target recomputes
	ThisUnit->FollowUnit = NewFollowTarget;


	UWorld* World = ThisUnit->GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	// Retrieve Mass entity handle from the binding component
	if (!ThisUnit->MassActorBindingComponent)
	{
		return;
	}
	const FMassEntityHandle MassEntityHandle = ThisUnit->MassActorBindingComponent->GetMassEntityHandle();
	if (!EntityManager.IsEntityValid(MassEntityHandle))
	{
		return;
	}

	// Set/clear the friendly follow target on the Mass AI target fragment
	if (FMassAITargetFragment* AITFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(MassEntityHandle))
	{
		if (NewFollowTarget && NewFollowTarget->MassActorBindingComponent)
		{
			const FMassEntityHandle TargetHandle = NewFollowTarget->MassActorBindingComponent->GetMassEntityHandle();
			if (EntityManager.IsEntityValid(TargetHandle))
			{
				AITFrag->FriendlyTargetEntity = TargetHandle;
				AITFrag->LastKnownFriendlyLocation = NewFollowTarget->GetMassActorLocation();
			}
			else
			{
				AITFrag->FriendlyTargetEntity.Reset();
				AITFrag->LastKnownFriendlyLocation = FVector::ZeroVector;
			}
		}
		else
		{
			AITFrag->FriendlyTargetEntity.Reset();
			AITFrag->TargetEntity.Reset();
			AITFrag->IsFocusedOnTarget = false;
			AITFrag->LastKnownFriendlyLocation = FVector::ZeroVector;
		}
	}

	// Immediately switch to appropriate state so movement starts instantly
	if (UMassSignalSubsystem* Signals = World->GetSubsystem<UMassSignalSubsystem>())
	{
		const bool bWantsRepair = (ThisUnit->IsWorker && ThisUnit->CanRepair && NewFollowTarget && NewFollowTarget->CanBeRepaired);
		if (bWantsRepair)
		{
			// Enter dedicated GoToRepair flow; specialized processors will drive follow updates
			Signals->SignalEntity(UnitSignals::GoToRepair, MassEntityHandle);
		}
		else if (NewFollowTarget)
		{
			Signals->SignalEntity(UnitSignals::Run, MassEntityHandle);
		}
		else
		{
			// Clearing follow: controllers will drive the next state/order; no signal here.
		}
	}
}
