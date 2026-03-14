// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Characters/Unit/MassUnitBase.h"

#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "GAS/AttributeSetBase.h"
#include "Mass/Signals/MySignals.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Mass/States/ChaseStateProcessor.h"
#include "MassCommonFragments.h"
#include "GameFramework/CharacterMovementComponent.h"
// For updating FMassMoveTargetFragment and prediction helpers
#include "Mass/UnitMassTag.h"
#include "MassNavigationFragments.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Subsystems/UnitVisualManager.h"
#include "Mass/MassUnitVisualFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"

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
		// ISMComponent->SetVisibility(false);
		ISMComponent->SetIsReplicated(true);
	}

	
	SelectionIcon = CreateDefaultSubobject<USelectionDecalComponent>(TEXT("SelectionIcon"));

	if (SelectionIcon)
	{
		SelectionIcon->SetupAttachment(RootComponent);
	}

	HealthWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Healthbar"));
	HealthWidgetComp->SetupAttachment(RootComponent);
	HealthWidgetComp->SetVisibility(true);
	HealthWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	float CapsuleHalfHeight = 88.f;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	HealthWidgetRelativeOffset = FVector(0.f, 0.f, CapsuleHalfHeight + HealthWidgetHeightOffset);
	HealthWidgetComp->SetRelativeLocation(HealthWidgetRelativeOffset);

	TimerWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Timer"));
	TimerWidgetComp->SetupAttachment(RootComponent);
	TimerWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TimerWidgetRelativeOffset = FVector(0.f, 0.f, CapsuleHalfHeight + TimerWidgetHeightOffset);
	TimerWidgetComp->SetRelativeLocation(TimerWidgetRelativeOffset);
}

void AMassUnitBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitializeUnitMode();
}

void AMassUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMassUnitBase, bIsMassUnit);
	DOREPLIFETIME(AMassUnitBase, ISMComponent);
	DOREPLIFETIME(AMassUnitBase, bUseSkeletalMovement);
	//DOREPLIFETIME(AMassUnitBase, bUseIsmWithActorMovement);

	DOREPLIFETIME(AMassUnitBase, Niagara_A);
	DOREPLIFETIME(AMassUnitBase, Niagara_B);

	DOREPLIFETIME(AMassUnitBase, Niagara_A_Start_Transform);
	DOREPLIFETIME(AMassUnitBase, Niagara_B_Start_Transform);
	
	DOREPLIFETIME(AMassUnitBase, MeshRotationOffset);
	
	DOREPLIFETIME(AMassUnitBase, IsFlying);
	DOREPLIFETIME(AMassUnitBase, FlyHeight);

	DOREPLIFETIME(AMassUnitBase, Rep_VE_ActiveEffects);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_PulsateMinScale);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_PulsateMaxScale);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_PulsateHalfPeriod);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_RotationAxis);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_RotationDegreesPerSecond);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_OscillationOffsetA);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_OscillationOffsetB);
	DOREPLIFETIME(AMassUnitBase, Rep_VE_OscillationCyclesPerSecond);

	DOREPLIFETIME(AMassUnitBase, HealthWidgetComp);
	DOREPLIFETIME(AMassUnitBase, TimerWidgetComp);
	DOREPLIFETIME(AMassUnitBase, HealthWidgetRelativeOffset);
	DOREPLIFETIME(AMassUnitBase, TimerWidgetRelativeOffset);
}


FVector AMassUnitBase::GetMassActorLocation() const
{
	if (!bUseSkeletalMovement)
	{
		const FMassEntityManager* EntityManager;
		FMassEntityHandle EntityHandle;
		if (GetMassEntityData(EntityManager, EntityHandle))
		{
			if (const FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
			{
				return CharFrag->PositionedTransform.GetLocation();
			}
		}
	}

	return Super::GetMassActorLocation();
}

FRotator AMassUnitBase::GetMassActorRotation() const
{
	if (!bUseSkeletalMovement)
	{
		const FMassEntityManager* EntityManager;
		FMassEntityHandle EntityHandle;
		if (GetMassEntityData(EntityManager, EntityHandle))
		{
			if (const FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
			{
				return CharFrag->PositionedTransform.GetRotation().Rotator();
			}
		}
	}

	return Super::GetMassActorRotation();
}

FTransform AMassUnitBase::GetMassActorTransform() const
{
	if (!bUseSkeletalMovement)
	{
		const FMassEntityManager* EntityManager;
		FMassEntityHandle EntityHandle;
		if (GetMassEntityData(EntityManager, EntityHandle))
		{
			if (const FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
			{
				return CharFrag->PositionedTransform;
			}
		}
	}

	return Super::GetMassActorTransform();
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

bool AMassUnitBase::AddStopSeparationTagToEntity()
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
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): AddStopSeparationTagToEntity failed - Entity %s is no longer valid."), *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}
	
	EntityManager->Defer().AddTag<FMassStateStopSeparationTag>(EntityHandle);
	
	return true;
}

bool AMassUnitBase::ApplyStopXYMovementTag(bool bApply)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle) || !EntityManager->IsEntityValid(EntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase (%s): ApplyStopXYMovementTag failed - invalid entity or manager."), *GetName());
		return false;
	}

	auto& Defer = EntityManager->Defer();
	if (bApply)
	{
		Defer.AddTag<FMassStateStopXYMovementTag>(EntityHandle);
	}
	else
	{
		Defer.RemoveTag<FMassStateStopXYMovementTag>(EntityHandle);
	}
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
	TargetFrag->LastKnownLocation = TargetUnit->GetMassActorLocation();
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


bool AMassUnitBase::UpdateEntityHealth(float NewHealth, float CurrentShield)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	
	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		return false;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		return false;
	}
	
	FMassCombatStatsFragment* CombatStats = EntityManager->GetFragmentDataPtr<FMassCombatStatsFragment>(EntityHandle);
	FMassVisibilityFragment* Vis = EntityManager->GetFragmentDataPtr<FMassVisibilityFragment>(EntityHandle);
	
	if (!CombatStats) return false;
	
	if (Attributes)
	{
		const float ActualShield = CurrentShield;
		
		// If we have a visibility fragment, handle logic-driven popups here (signal-driven)
		if (Vis)
		{
			// Only trigger if not the very first sync
			if (Vis->LastHealth >= 0.f)
			{
				// Significant changes trigger the healthbar popup timer
				// Use same logic as previous processor: negative delta > 1.0 or positive delta > HealThreshold
				const bool bIsClient = (GetWorld()->GetNetMode() == NM_Client);
				const float HealThreshold = 10.0f; 

				const bool bDamageTrigger = (NewHealth < Vis->LastHealth - 1.0f) || (ActualShield < Vis->LastShield - 1.0f);
				const bool bHealTrigger = (NewHealth > Vis->LastHealth + HealThreshold) || (ActualShield > Vis->LastShield + HealThreshold);

				if (bDamageTrigger || bHealTrigger)
				{
					Vis->LastHealthChangeTime = GetWorld()->GetTimeSeconds();
				}

				// Always update LastHealth/LastShield to track current state correctly.
				// The previous 'restrictive' update logic caused values to get stuck, leading to 
				// continuous damage triggers during regeneration.
				Vis->LastHealth = NewHealth;
				Vis->LastShield = ActualShield;
			}
			else // First initialization
			{
				Vis->LastHealth = NewHealth;
				Vis->LastShield = ActualShield;
			}
		}

		//CombatStats->Health = NewHealth;
		//CombatStats->Shield = ActualShield;
	}

	if (CombatStats->Health <= 0.f)
	{
		SwitchEntityTag(FMassStateDeadTag::StaticStruct());
	}

	return true;
}


bool AMassUnitBase::UpdateLevelUpTimestamp()
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;

	if (!GetMassEntityData(EntityManager, EntityHandle))
	{
		return false;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		return false;
	}

	FMassVisibilityFragment* Vis = EntityManager->GetFragmentDataPtr<FMassVisibilityFragment>(EntityHandle);

	if (!Vis) return false;

	if (UWorld* World = GetWorld())
	{
		Vis->LastLevelUpTime = World->GetTimeSeconds();
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

	OutEntityManager = &EntitySubsystem->GetMutableEntityManager();
	return true;
}

bool AMassUnitBase::GetMassEntityData(const FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle) const
{
	OutEntityManager = nullptr;
	OutEntityHandle.Reset();

	const UWorld* World = GetWorld();
	if (!World) return false;

	const UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return false;

	if (!MassActorBindingComponent) return false;

	OutEntityHandle = MassActorBindingComponent->GetEntityHandle();
	if (!OutEntityHandle.IsSet()) return false;

	OutEntityManager = &EntitySubsystem->GetEntityManager();
	return true;
}

void AMassUnitBase::BeginPlay()
{
	Super::BeginPlay();
}

void AMassUnitBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}


void AMassUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bMassVisualsRegistered = false;

	if (UUnitVisualManager* VisualManager = GetWorld() ? GetWorld()->GetSubsystem<UUnitVisualManager>() : nullptr)
	{
		if (MassActorBindingComponent)
		{
			FMassEntityHandle EntityHandle = MassActorBindingComponent->GetEntityHandle();
			if (EntityHandle.IsValid())
			{
				VisualManager->RemoveUnitVisual(EntityHandle);
			}
		}
	}

	// Stop any pending rotation/movement timers for static meshes/niagara
	GetWorldTimerManager().ClearTimer(StaticMeshRotateTimerHandle);
	GetWorldTimerManager().ClearTimer(StaticMeshYawFollowTimerHandle);
	ActiveStaticMeshTweens.Empty();
	ActiveYawFollows.Empty();

	GetWorldTimerManager().ClearTimer(StaticMeshMoveTimerHandle);
	ActiveStaticMeshMoveTweens.Empty();

	// Stop any pending scaling timers
	GetWorldTimerManager().ClearTimer(StaticMeshScaleTimerHandle);
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
		if (GetMesh()) GetMesh()->SetVisibility(true);
		if (ISMComponent) ISMComponent->SetVisibility(false);
	}
	else
	{
		if (GetMesh()) GetMesh()->SetVisibility(false);
		if (ISMComponent) ISMComponent->SetVisibility(true);
	}

	
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
			ISMComponent->UpdateInstanceTransform(InstanceIndex, LocalIdentityTransform, false, true, false);
			MeshRotationOffset = ISMComponent->GetRelativeRotation().Quaternion().GetNormalized();
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
}

bool AMassUnitBase::RegisterVisualsToMass()
{
	if (!GetWorld() || !GetWorld()->IsGameWorld() || bMassVisualsRegistered) return true;

	if (!bUseSkeletalMovement)
	{
		UCharacterMovementComponent* MoveComp = GetCharacterMovement();
		if (MoveComp)
		{
			MoveComp->Deactivate();
			MoveComp->SetComponentTickEnabled(false);
		}
		
		if (ISMComponent)
		{
			if (ISMComponent->GetStaticMesh())
			{
				UUnitVisualManager* VisualManager = GetWorld()->GetSubsystem<UUnitVisualManager>();
				if (VisualManager && MassActorBindingComponent)
				{
					FMassEntityHandle EntityHandle = MassActorBindingComponent->GetEntityHandle();
					if (EntityHandle.IsValid())
					{
 					VisualManager->AssignUnitVisual(EntityHandle, ISMComponent, this);

						if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
						{
							EffectFrag->bPulsateEnabled = (Rep_VE_ActiveEffects & (1 << 0)) != 0;
							EffectFrag->PulsateMinScale = Rep_VE_PulsateMinScale;
							EffectFrag->PulsateMaxScale = Rep_VE_PulsateMaxScale;
							EffectFrag->PulsateHalfPeriod = Rep_VE_PulsateHalfPeriod;

							EffectFrag->bRotationEnabled = (Rep_VE_ActiveEffects & (1 << 1)) != 0;
							EffectFrag->RotationAxis = Rep_VE_RotationAxis;
							EffectFrag->RotationDegreesPerSecond = Rep_VE_RotationDegreesPerSecond;

							EffectFrag->bOscillationEnabled = (Rep_VE_ActiveEffects & (1 << 2)) != 0;
							EffectFrag->OscillationOffsetA = Rep_VE_OscillationOffsetA;
							EffectFrag->OscillationOffsetB = Rep_VE_OscillationOffsetB;
							EffectFrag->OscillationCyclesPerSecond = Rep_VE_OscillationCyclesPerSecond;

							EffectFrag->PulsateTargetISM = ISMComponent;
							EffectFrag->RotationTargetISM = ISMComponent;
							EffectFrag->OscillationTargetISM = ISMComponent;
						}

						return true;
					}
					UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase::RegisterVisualsToMass: EntityHandle is invalid for unit %s"), *GetName());
					return false;
				}
				UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase::RegisterVisualsToMass: VisualManager or MassActorBindingComponent is null for unit %s"), *GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase::RegisterVisualsToMass: ISMComponent has no static mesh for unit %s"), *GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase::RegisterVisualsToMass: ISMComponent is null for unit %s"), *GetName());
		}
	}
	return true;
}

bool AMassUnitBase::RegisterAdditionalVisualsToMass()
{
	if (!GetWorld() || !GetWorld()->IsGameWorld() || bMassVisualsRegistered) return true;

	if (AdditionalISMComponents.Num() == 0) return true;

	UE_LOG(LogTemp, Log, TEXT("AMassUnitBase::RegisterAdditionalVisualsToMass: Called for unit %s with %d additional ISMs"), *GetName(), AdditionalISMComponents.Num());

	UUnitVisualManager* VisualManager = GetWorld()->GetSubsystem<UUnitVisualManager>();
	if (VisualManager && MassActorBindingComponent)
	{
		FMassEntityHandle EntityHandle = MassActorBindingComponent->GetEntityHandle();
		if (EntityHandle.IsValid())
		{
			for (UInstancedStaticMeshComponent* Comp : AdditionalISMComponents)
			{
				if (Comp)
				{
					UE_LOG(LogTemp, Log, TEXT("AMassUnitBase::RegisterAdditionalVisualsToMass: Assigning additional ISM %s for unit %s to entity %s"), *Comp->GetName(), *GetName(), *EntityHandle.DebugGetDescription());
					VisualManager->AssignUnitVisual(EntityHandle, Comp, this);
				}
			}
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase::RegisterAdditionalVisualsToMass: EntityHandle is invalid for unit %s"), *GetName());
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("AMassUnitBase::RegisterAdditionalVisualsToMass: VisualManager or MassActorBindingComponent is null for unit %s"), *GetName());
	return true;
}

void AMassUnitBase::RemoveAdditionalISMInstances()
{
	// Process main ISMComponent
	if (ISMComponent)
	{
		ISMComponent->SetVisibility(false);
		ISMComponent->SetCastShadow(false);
		ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (ISMComponent->GetInstanceCount() > 0)
		{
			ISMComponent->ClearInstances();
		}
		InstanceIndex = INDEX_NONE;
	}

	// Process additional ISMs
	for (UInstancedStaticMeshComponent* Comp : AdditionalISMComponents)
	{
		if (Comp)
		{
			Comp->SetVisibility(false);
			Comp->SetCastShadow(false);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (Comp->GetInstanceCount() > 0)
			{
				Comp->ClearInstances();
			}
		}
	}
}

int32 AMassUnitBase::InitializeAdditionalISM(UInstancedStaticMeshComponent* InISMComponent)
{
	if (!InISMComponent)
	{
		return INDEX_NONE;
	}

	AdditionalISMComponents.AddUnique(InISMComponent);

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		// Deactivate local rendering/collision immediately to avoid double rendering
		InISMComponent->SetVisibility(false);
		InISMComponent->SetCastShadow(false);
		InISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		// Still create a local instance in Editor for visibility
		if (InISMComponent->GetInstanceCount() == 0)
		{
			const FTransform LocalIdentityTransform = FTransform::Identity;
			return InISMComponent->AddInstance(LocalIdentityTransform, false);
		}
	}

	return 0; // We don't have a Mass index yet
}

bool AMassUnitBase::GetMassVisualInstance(UInstancedStaticMeshComponent* TemplateISM, UInstancedStaticMeshComponent*& OutComponent, int32& OutInstanceIndex) const
{
	if (const FMassUnitVisualFragment* VisualFrag = GetVisualFragment())
	{
		for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
		{
			if (Instance.TemplateISM == TemplateISM)
			{
				OutComponent = Instance.TargetISM.Get();
				OutInstanceIndex = Instance.InstanceIndex;
				return OutComponent != nullptr && OutInstanceIndex != INDEX_NONE;
			}
		}
	}
	return false;
}

FMassUnitVisualFragment* AMassUnitBase::GetMutableVisualFragment()
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (GetMassEntityData(EntityManager, EntityHandle))
	{
		return EntityManager->GetFragmentDataPtr<FMassUnitVisualFragment>(EntityHandle);
	}
	return nullptr;
}

const FMassUnitVisualFragment* AMassUnitBase::GetVisualFragment() const
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	// Use the non-const GetMassEntityData by temporarily casting away const
	if (const_cast<AMassUnitBase*>(this)->GetMassEntityData(EntityManager, EntityHandle))
	{
		return EntityManager->GetFragmentDataPtr<FMassUnitVisualFragment>(EntityHandle);
	}
	return nullptr;
}

FMassVisualTweenFragment* AMassUnitBase::GetMutableTweenFragment()
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (GetMassEntityData(EntityManager, EntityHandle))
	{
		return EntityManager->GetFragmentDataPtr<FMassVisualTweenFragment>(EntityHandle);
	}
	return nullptr;
}

FMassVisualEffectFragment* AMassUnitBase::GetMutableEffectFragment()
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (GetMassEntityData(EntityManager, EntityHandle))
	{
		return EntityManager->GetFragmentDataPtr<FMassVisualEffectFragment>(EntityHandle);
	}
	return nullptr;
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
	Current.SetTranslation(GetActorLocation()); Current.SetRotation(GetActorRotation().Quaternion()); Current.SetScale3D(GetActorScale3D());

	// Mark dirty so PlacementProcessor picks it up
	if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
	{
		CharFrag->PositionedTransform = Current;
		CharFrag->bTransformDirty = true;
	}

	return true;
}


bool AMassUnitBase::SyncRotation()
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
			   TEXT("AMassUnitBase (%s): SyncRotation failed – entity %s invalid."),
			   *GetName(), *EntityHandle.DebugGetDescription());
		return false;
	}

	// Fetch the transform fragment and overwrite its rotation
	FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (!TransformFrag)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("AMassUnitBase (%s): SyncRotation failed – no FTransformFragment found."),
			   *GetName());
		return false;
	}

	// Sync
	FTransform& Current = TransformFrag->GetMutableTransform();
	Current.SetRotation(GetActorRotation().Quaternion());

	// Mark dirty so PlacementProcessor picks it up
	if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
	{
		CharFrag->PositionedTransform.SetRotation(Current.GetRotation());
		CharFrag->bTransformDirty = true;
	}

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

	// Mark dirty so PlacementProcessor picks it up
	if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
	{
		CharFrag->PositionedTransform.SetTranslation(NewLocation);
		CharFrag->bTransformDirty = true;
	}

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

void AMassUnitBase::Multicast_UpdateISMInstanceTransform_Implementation(int32 InstIndex, const FTransform& NewTransform)
{
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	if (const FMassUnitVisualFragment* VisualFrag = GetVisualFragment())
	{
		if (VisualFrag->VisualInstances.IsValidIndex(InstIndex))
		{
			const FMassUnitVisualInstance& Data = VisualFrag->VisualInstances[InstIndex];
			TargetISM = Data.TargetISM.Get();
			TargetInstIndex = Data.InstanceIndex;
		}
	}

	// Prioritize Mass Manager Visuals
	if (TargetISM && TargetInstIndex != INDEX_NONE)
	{
		TargetISM->UpdateInstanceTransform(TargetInstIndex, NewTransform, true, false, false);
		TargetISM->MarkRenderDynamicDataDirty();
	}
	else
	{
		if (this->ISMComponent && this->ISMComponent->IsValidInstance(InstIndex))
		{
			this->ISMComponent->UpdateInstanceTransform(InstIndex, NewTransform, true, false, false);
			this->ISMComponent->MarkRenderDynamicDataDirty();
		}
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

bool AMassUnitBase::SetPathWaypoints(const TArray<FVector>& NewPoints, bool bClearExistingFirst, bool bAttackToggled)
{
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

	FMassUnitPathFragment* PathFrag = EntityManager->GetFragmentDataPtr<FMassUnitPathFragment>(EntityHandle);
	if (!PathFrag)
	{
		return false;
	}

	if (bClearExistingFirst)
	{
		PathFrag->Waypoints.Reset();
		PathFrag->CurrentIndex = 0;
	}

	PathFrag->bAttackToggled = bAttackToggled;

	for (const FVector& P : NewPoints)
	{
		if (PathFrag->Waypoints.Num() < 10)
		{
			PathFrag->Waypoints.Add(P);
		}
		else
		{
			break;
		}
	}
	return true;
}

bool AMassUnitBase::ClearPathWaypoints()
{
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
	FMassUnitPathFragment* PathFrag = EntityManager->GetFragmentDataPtr<FMassUnitPathFragment>(EntityHandle);
	if (!PathFrag)
	{
		return false;
	}
	PathFrag->Waypoints.Reset();
	PathFrag->CurrentIndex = 0;
	return true;
}

void AMassUnitBase::MulticastTransformSync_Implementation(const FVector& Location)
{
	//SetActorLocation(Location);
	SetTranslationLocation(Location);
	UpdatePredictionFragment(Location, Attributes->GetBaseRunSpeed());
	StopMassMovement();
	SwitchEntityTagByState(UnitData::Idle, UnitStatePlaceholder);
}

void AMassUnitBase::MulticastRotateISMLinear_Implementation(const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent, UInstancedStaticMeshComponent* InISMComponent)
{
	if (FMassVisualTweenFragment* TweenFrag = GetMutableTweenFragment())
	{
		TweenFrag->RotationTween.StartRotation = GetCurrentLocalVisualRotation(InISMComponent);
		TweenFrag->RotationTween.TargetRotation = NewRotation.Quaternion().GetNormalized();
		TweenFrag->RotationTween.Duration = InRotateDuration;
		TweenFrag->RotationTween.Elapsed = 0.f;
		TweenFrag->RotationTween.EaseExp = FMath::Max(InRotationEaseExponent, 0.001f);
		TweenFrag->RotationTween.bActive = true;
		TweenFrag->RotationTween.TargetISM = InISMComponent ? InISMComponent : ISMComponent;

		// Shortest arc
		if ((TweenFrag->RotationTween.StartRotation | TweenFrag->RotationTween.TargetRotation) < 0.f)
		{
			TweenFrag->RotationTween.TargetRotation = (-TweenFrag->RotationTween.TargetRotation);
		}
	}
}

FQuat AMassUnitBase::GetCurrentLocalVisualRotation(UInstancedStaticMeshComponent* InISM) const
{
	UInstancedStaticMeshComponent* TemplateISM = InISM ? InISM : ISMComponent;
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	// Prioritize Mass Manager Visuals
	if (GetMassVisualInstance(TemplateISM, TargetISM, TargetInstIndex))
	{
		// Try to get current instance rotation if it's not default/zero
		FTransform InstanceXf;
		if (TargetISM->GetInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false))
		{
			// We check if the transform is valid/not completely zeroed out by initialization
			if (!InstanceXf.GetRotation().IsIdentity())
			{
				return InstanceXf.GetRotation().GetNormalized();
			}
		}

		// If instance transform is not helpful, use the BaseOffset from the fragment
		if (const FMassUnitVisualFragment* VisualFrag = GetVisualFragment())
		{
			for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
			{
				if (Instance.TemplateISM == TemplateISM)
				{
					return Instance.BaseOffset.GetRotation().GetNormalized();
				}
			}
		}
	}

	if (!bUseSkeletalMovement && TemplateISM)
	{
		// If it's the main ISM component and we have a valid index
		if (TemplateISM == ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			return InstanceXf.GetRotation().GetNormalized();
		}
		
		// Fallback to component's relative rotation
		return TemplateISM->GetRelativeRotation().Quaternion().GetNormalized();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeRotation().Quaternion().GetNormalized();
	}

	return GetActorQuat().GetNormalized();
}

void AMassUnitBase::ApplyLocalVisualRotation(const FQuat& NewLocalRotation, UInstancedStaticMeshComponent* InISM)
{
	UInstancedStaticMeshComponent* TemplateISM = InISM ? InISM : ISMComponent;
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	// Prioritize Mass Manager Visuals
	if (GetMassVisualInstance(TemplateISM, TargetISM, TargetInstIndex))
	{
		const FQuat Visual = NewLocalRotation.GetNormalized();
		FTransform InstanceXf;
		TargetISM->GetInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false);
		InstanceXf.SetRotation(Visual);
		TargetISM->UpdateInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
		return;
	}

	// Fallback to local component
	if (!bUseSkeletalMovement && TemplateISM)
	{
		const FQuat Visual = NewLocalRotation.GetNormalized();
		if (TemplateISM == ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			InstanceXf.SetRotation(Visual);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		TemplateISM->SetRelativeRotation(Visual.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeRotation(NewLocalRotation.Rotator(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
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

void AMassUnitBase::MulticastRotateNiagaraLinear_Implementation(UNiagaraComponent* NiagaraToRotate, const FRotator& NewRotation, float InRotateDuration, float InRotationEaseExponent)
{
	if (!NiagaraToRotate)
	{
		return;
	}

	// Instant snap if duration <= 0
	if (InRotateDuration <= 0.f)
	{
		NiagaraToRotate->SetRelativeRotation(NewRotation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Setup or update tween for this component
	AMassUnitBase::FStaticMeshRotateTween& Tween = ActiveNiagaraTweens.FindOrAdd(NiagaraToRotate);
	Tween.Duration = InRotateDuration;
	Tween.Elapsed = 0.f;
	Tween.EaseExp = FMath::Max(InRotationEaseExponent, 0.001f);
	Tween.Start = NiagaraToRotate->GetRelativeRotation().Quaternion().GetNormalized();
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
	NiagaraRotations_Step();

	if (!GetWorldTimerManager().IsTimerActive(NiagaraRotateTimerHandle))
	{
		GetWorldTimerManager().SetTimer(NiagaraRotateTimerHandle, this, &AMassUnitBase::NiagaraRotations_Step, TimerRate, true);
	}
}

void AMassUnitBase::NiagaraRotations_Step()
{
	if (ActiveNiagaraTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(NiagaraRotateTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	TArray<TWeakObjectPtr<UNiagaraComponent>> ToRemove;
	for (auto& Pair : ActiveNiagaraTweens)
	{
		TWeakObjectPtr<UNiagaraComponent> WeakComp = Pair.Key;
		FStaticMeshRotateTween& Tween = Pair.Value;

		UNiagaraComponent* Comp = WeakComp.Get();
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
		ActiveNiagaraTweens.Remove(Key);
	}

	if (ActiveNiagaraTweens.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(NiagaraRotateTimerHandle);
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

void AMassUnitBase::MulticastRotateISMYawToChase_Implementation(UInstancedStaticMeshComponent* ISMToRotate, int32 InstIndex, float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees, bool bTeleport)
{
	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->bYawChaseEnabled = bEnable;
		EffectFrag->YawChaseOffset = YawOffsetDegrees;
		EffectFrag->YawChaseDuration = InRotateDuration;
		EffectFrag->YawChaseTargetISM = ISMToRotate ? ISMToRotate : ISMComponent;
		
		AUnitBase* ThisUnit = Cast<AUnitBase>(this);
		if (ThisUnit && ThisUnit->UnitToChase)
		{
			if (AMassUnitBase* MassTarget = Cast<AMassUnitBase>(ThisUnit->UnitToChase))
			{
				FMassEntityManager* TargetEM;
				FMassEntityHandle TargetHandle;
				if (MassTarget->GetMassEntityData(TargetEM, TargetHandle))
				{
					EffectFrag->TargetEntity = TargetHandle;
				}
			}
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
		float DesiredWorldYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + Data.OffsetDegrees);

		if (IsValid(TargetUnit))
		{
			bool bIsDead = false;
			if (AMassUnitBase* MassTarget = Cast<AMassUnitBase>(TargetUnit))
			{
				FMassEntityManager* TargetEM;
				FMassEntityHandle TargetHandle;
				if (MassTarget->GetMassEntityData(TargetEM, TargetHandle))
				{
					bIsDead = DoesEntityHaveTag(*TargetEM, TargetHandle, FMassStateDeadTag::StaticStruct());
				}
			}

			const float Distance = FVector::Dist(GetActorLocation(), TargetUnit->GetActorLocation());
			const float MaxRange = MassActorBindingComponent ? MassActorBindingComponent->LoseSightRadius : 2500.f;

			if (!bIsDead && Distance <= MaxRange)
			{
				const FVector CompWorldLoc = Comp->GetComponentLocation();
				FVector TargetLoc = TargetUnit->GetActorLocation();

				// 2D direction on the XY plane
				FVector ToTarget = TargetLoc - CompWorldLoc;
				ToTarget.Z = 0.f;
				if (!ToTarget.IsNearlyZero(1e-3f))
				{
					const FRotator FacingRot = FRotationMatrix::MakeFromX(ToTarget.GetSafeNormal()).Rotator();
					DesiredWorldYaw = FRotator::NormalizeAxis(FacingRot.Yaw + Data.OffsetDegrees);
				}
			}
		}

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

void AMassUnitBase::MulticastRotateUnitYawToChase_Implementation(float InRotateDuration, float InRotationEaseExponent, bool bEnable, float YawOffsetDegrees)
{
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	if (GetMassEntityData(EntityManager, EntityHandle))
	{
		if (bEnable)
		{
			// Enable the processor via Tag
			EntityManager->Defer().AddTag<FMassUnitYawFollowTag>(EntityHandle);
			
			// Provide the rotation parameters to the fragment
			if (FMassUnitYawFollowFragment* FollowFrag = EntityManager->GetFragmentDataPtr<FMassUnitYawFollowFragment>(EntityHandle))
			{
				FollowFrag->Duration = FMath::Max(InRotateDuration, 0.001f);
				FollowFrag->EaseExp = FMath::Max(InRotationEaseExponent, 0.001f);
				FollowFrag->OffsetDegrees = YawOffsetDegrees;
			}
		}
		else
		{
			// Disable the processor
			EntityManager->Defer().RemoveTag<FMassUnitYawFollowTag>(EntityHandle);
		}
	}

}

void AMassUnitBase::MulticastContinuousUnitRotation_Implementation(float YawRate, bool bEnable)
{
	// Update persistent state on Server
	if (HasAuthority())
	{
		if (bEnable) Rep_VE_ActiveEffects |= (1 << 1);
		else Rep_VE_ActiveEffects &= ~(1 << 1);
		Rep_VE_RotationDegreesPerSecond = YawRate;
		Rep_VE_RotationAxis = FVector::UpVector;
	}

	bContinuousUnitRotationEnabled = bEnable;
	ContinuousUnitYawRate = YawRate;

	if (bEnable)
	{
		// Disable unit-to-chase yaw follow if it was active
		FMassEntityManager* EntityManager;
		FMassEntityHandle EntityHandle;
		if (GetMassEntityData(EntityManager, EntityHandle))
		{
			EntityManager->Defer().RemoveTag<FMassUnitYawFollowTag>(EntityHandle);
		}

		// Handle Actor spin (for non-Mass units or skeletal meshes)
		if (bUseSkeletalMovement || !bIsMassUnit)
		{
			if (!GetWorldTimerManager().IsTimerActive(ContinuousUnitRotationTimerHandle))
			{
				const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
				const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
				GetWorldTimerManager().SetTimer(ContinuousUnitRotationTimerHandle, this, &AMassUnitBase::ContinuousUnitRotation_Step, TimerRate, true);
			}
		}
		else
		{
			GetWorldTimerManager().ClearTimer(ContinuousUnitRotationTimerHandle);
		}

		// Handle ISM spin via EffectFragment (for Mass units)
		if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
		{
			EffectFrag->bRotationEnabled = true;
			EffectFrag->RotationAxis = FVector::UpVector;
			EffectFrag->RotationDegreesPerSecond = YawRate;
			EffectFrag->RotationDuration = -1.f;
			EffectFrag->RotationElapsed = 0.f;
			EffectFrag->RotationTargetISM = nullptr; // Apply to all instances for whole unit rotation
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(ContinuousUnitRotationTimerHandle);

		// Also disable ISM continuous rotation
		if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
		{
			EffectFrag->bRotationEnabled = false;
		}
	}
}

void AMassUnitBase::ContinuousUnitRotation_Step()
{
	if (!bContinuousUnitRotationEnabled)
	{
		GetWorldTimerManager().ClearTimer(ContinuousUnitRotationTimerHandle);
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw += ContinuousUnitYawRate * Dt;
	NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw);

	SetActorRotation(NewRotation);
	SyncRotation();
}

void AMassUnitBase::MulticastMoveUnitLinear_Implementation(const FVector& RelativeLocationChange, float InMoveDuration, float InMoveEaseExponent)
{
	const FVector WorldOffset = GetActorRotation().RotateVector(RelativeLocationChange);

	// Snap if no duration
	if (InMoveDuration <= 0.f)
	{
		const FVector TargetLoc = GetActorLocation() + WorldOffset;
		SetActorLocation(TargetLoc, false, nullptr, ETeleportType::TeleportPhysics);
		SyncTranslation();
		return;
	}

	// Initialize interpolation state
	GetWorldTimerManager().ClearTimer(UnitMoveTimerHandle);
	UnitMoveTween.Duration = InMoveDuration;
	UnitMoveTween.Elapsed = 0.f;
	UnitMoveTween.EaseExp = FMath::Max(InMoveEaseExponent, 0.001f);
	UnitMoveTween.Start = GetActorLocation();
	UnitMoveTween.Target = UnitMoveTween.Start + WorldOffset;

	// Immediate step then start timer
	UnitMove_Step();

	const float FrameDt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const float TimerRate = (FrameDt > 0.f) ? FrameDt : (1.f / 60.f);
	GetWorldTimerManager().SetTimer(UnitMoveTimerHandle, this, &AMassUnitBase::UnitMove_Step, TimerRate, true);
}

void AMassUnitBase::UnitMove_Step()
{
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	UnitMoveTween.Elapsed += Dt;

	float Alpha = (UnitMoveTween.Duration > 0.f) ? FMath::Clamp(UnitMoveTween.Elapsed / UnitMoveTween.Duration, 0.f, 1.f) : 1.f;
	if (!FMath::IsNearlyEqual(UnitMoveTween.EaseExp, 1.f))
	{
		Alpha = FMath::Pow(Alpha, UnitMoveTween.EaseExp);
	}

	const FVector NewLocation = FMath::Lerp(UnitMoveTween.Start, UnitMoveTween.Target, Alpha);
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SyncTranslation();

	if (Alpha >= 1.f)
	{
		SetActorLocation(UnitMoveTween.Target, false, nullptr, ETeleportType::TeleportPhysics);
		SyncTranslation();
		GetWorldTimerManager().ClearTimer(UnitMoveTimerHandle);
	}
}

void AMassUnitBase::MulticastMoveISMLinear_Implementation(const FVector& NewLocation, float InMoveDuration, float InMoveEaseExponent, UInstancedStaticMeshComponent* InISMComponent)
{
	if (FMassVisualTweenFragment* TweenFrag = GetMutableTweenFragment())
	{
		TweenFrag->LocationTween.StartLocation = GetCurrentLocalVisualLocation(InISMComponent);
		TweenFrag->LocationTween.TargetLocation = NewLocation;
		TweenFrag->LocationTween.Duration = InMoveDuration;
		TweenFrag->LocationTween.Elapsed = 0.f;
		TweenFrag->LocationTween.EaseExp = FMath::Max(InMoveEaseExponent, 0.001f);
		TweenFrag->LocationTween.bActive = true;
		TweenFrag->LocationTween.TargetISM = InISMComponent ? InISMComponent : ISMComponent;
	}
}

void AMassUnitBase::MulticastContinuousISMRotation_Implementation(float YawRate, bool bEnable, float Duration, UInstancedStaticMeshComponent* InISMComponent)
{
	if (HasAuthority())
	{
		if (bEnable) Rep_VE_ActiveEffects |= (1 << 1);
		else Rep_VE_ActiveEffects &= ~(1 << 1);
		Rep_VE_RotationDegreesPerSecond = YawRate;
		Rep_VE_RotationAxis = FVector::UpVector;
	}

	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->bRotationEnabled = bEnable;
		if (bEnable)
		{
			EffectFrag->RotationAxis = FVector::UpVector;
			EffectFrag->RotationDegreesPerSecond = YawRate;
			EffectFrag->RotationDuration = Duration;
			EffectFrag->RotationElapsed = 0.f;
			EffectFrag->RotationTargetISM = InISMComponent ? InISMComponent : ISMComponent;
		}
	}
}

void AMassUnitBase::MulticastMoveISMLinearRelative_Implementation(const FVector& RelativeLocationChange, float InMoveDuration, float InMoveEaseExponent, UInstancedStaticMeshComponent* InISMComponent)
{
	if (FMassVisualTweenFragment* TweenFrag = GetMutableTweenFragment())
	{
		FVector CurrentLoc = GetCurrentLocalVisualLocation(InISMComponent);
		TweenFrag->LocationTween.StartLocation = CurrentLoc;
		TweenFrag->LocationTween.TargetLocation = CurrentLoc + RelativeLocationChange;
		TweenFrag->LocationTween.Duration = InMoveDuration;
		TweenFrag->LocationTween.Elapsed = 0.f;
		TweenFrag->LocationTween.EaseExp = FMath::Max(InMoveEaseExponent, 0.001f);
		TweenFrag->LocationTween.bActive = true;
		TweenFrag->LocationTween.TargetISM = InISMComponent ? InISMComponent : ISMComponent;
	}
}

void AMassUnitBase::MulticastISMTransformSync_Implementation(const FVector& Location, const FRotator& Rotation, const FVector& Scale, UInstancedStaticMeshComponent* InISMComponent)
{
	if (FMassVisualTweenFragment* TweenFrag = GetMutableTweenFragment())
	{
		UInstancedStaticMeshComponent* TargetISM = InISMComponent ? InISMComponent : ISMComponent;

		// Snap location
		TweenFrag->LocationTween.StartLocation = Location;
		TweenFrag->LocationTween.TargetLocation = Location;
		TweenFrag->LocationTween.Duration = 0.f;
		TweenFrag->LocationTween.Elapsed = 0.f;
		TweenFrag->LocationTween.bActive = false;
		TweenFrag->LocationTween.TargetISM = TargetISM;

		// Snap rotation
		TweenFrag->RotationTween.StartRotation = Rotation.Quaternion().GetNormalized();
		TweenFrag->RotationTween.TargetRotation = Rotation.Quaternion().GetNormalized();
		TweenFrag->RotationTween.Duration = 0.f;
		TweenFrag->RotationTween.Elapsed = 0.f;
		TweenFrag->RotationTween.bActive = false;
		TweenFrag->RotationTween.TargetISM = TargetISM;

		// Snap scale
		TweenFrag->ScaleTween.StartScale = Scale;
		TweenFrag->ScaleTween.TargetScale = Scale;
		TweenFrag->ScaleTween.Duration = 0.f;
		TweenFrag->ScaleTween.Elapsed = 0.f;
		TweenFrag->ScaleTween.bActive = false;
		TweenFrag->ScaleTween.TargetISM = TargetISM;
	}

	// Also directly apply to the visual fragment so PlacementProcessor picks it up immediately
	if (FMassUnitVisualFragment* VisualFrag = GetMutableVisualFragment())
	{
		UInstancedStaticMeshComponent* TargetISM = InISMComponent ? InISMComponent : ISMComponent;
		for (FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
		{
			if (!Instance.TemplateISM.IsValid()) continue;
			if (Instance.TemplateISM == TargetISM)
			{
				FTransform NewTransform;
				NewTransform.SetLocation(Location);
				NewTransform.SetRotation(Rotation.Quaternion().GetNormalized());
				NewTransform.SetScale3D(Scale);
				Instance.CurrentRelativeTransform = NewTransform;
			}
		}
	}

	// Mark transform dirty so PlacementProcessor updates this frame
	FMassEntityManager* EntityManager;
	FMassEntityHandle EntityHandle;
	if (GetMassEntityData(EntityManager, EntityHandle))
	{
		if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager->GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
		{
			CharFrag->bTransformDirty = true;
		}
	}
}

FVector AMassUnitBase::GetCurrentLocalVisualLocation(UInstancedStaticMeshComponent* InISM) const
{
	UInstancedStaticMeshComponent* TemplateISM = InISM ? InISM : ISMComponent;
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	// Prioritize Mass Manager Visuals
	if (GetMassVisualInstance(TemplateISM, TargetISM, TargetInstIndex))
	{
		FTransform InstanceXf;
		if (TargetISM->GetInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false))
		{
			if (!InstanceXf.GetLocation().IsNearlyZero())
			{
				return InstanceXf.GetLocation();
			}
		}

		if (const FMassUnitVisualFragment* VisualFrag = GetVisualFragment())
		{
			for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
			{
				if (Instance.TemplateISM == TemplateISM)
				{
					return Instance.BaseOffset.GetLocation();
				}
			}
		}
	}

	if (!bUseSkeletalMovement && TemplateISM)
	{
		if (TemplateISM == ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			return InstanceXf.GetLocation();
		}
		return TemplateISM->GetRelativeLocation();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeLocation();
	}

	return GetActorLocation();
}

void AMassUnitBase::ApplyLocalVisualLocation(const FVector& NewLocalLocation, UInstancedStaticMeshComponent* InISM)
{
	UInstancedStaticMeshComponent* TemplateISM = InISM ? InISM : ISMComponent;
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	// Prioritize Mass Manager Visuals
	if (GetMassVisualInstance(TemplateISM, TargetISM, TargetInstIndex))
	{
		FTransform InstanceXf;
		TargetISM->GetInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false);
		InstanceXf.SetLocation(NewLocalLocation);
		TargetISM->UpdateInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
		return;
	}

	if (!bUseSkeletalMovement && TemplateISM)
	{
		if (TemplateISM == ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			InstanceXf.SetLocation(NewLocalLocation);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		TemplateISM->SetRelativeLocation(NewLocalLocation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeLocation(NewLocalLocation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
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

void AMassUnitBase::MulticastScaleISMLinear_Implementation(const FVector& NewScale, float InScaleDuration, float InScaleEaseExponent, UInstancedStaticMeshComponent* InISMComponent)
{
	if (FMassVisualTweenFragment* TweenFrag = GetMutableTweenFragment())
	{
		TweenFrag->ScaleTween.StartScale = GetCurrentLocalVisualScale(InISMComponent);
		TweenFrag->ScaleTween.TargetScale = NewScale;
		TweenFrag->ScaleTween.Duration = InScaleDuration;
		TweenFrag->ScaleTween.Elapsed = 0.f;
		TweenFrag->ScaleTween.EaseExp = FMath::Max(InScaleEaseExponent, 0.001f);
		TweenFrag->ScaleTween.bActive = true;
		TweenFrag->ScaleTween.TargetISM = InISMComponent ? InISMComponent : ISMComponent;
	}
}

FVector AMassUnitBase::GetCurrentLocalVisualScale(UInstancedStaticMeshComponent* InISM) const
{
	UInstancedStaticMeshComponent* TemplateISM = InISM ? InISM : ISMComponent;
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	// Prioritize Mass Manager Visuals
	if (GetMassVisualInstance(TemplateISM, TargetISM, TargetInstIndex))
	{
		FTransform InstanceXf;
		if (TargetISM->GetInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false))
		{
			if (!InstanceXf.GetScale3D().IsNearlyZero())
			{
				return InstanceXf.GetScale3D();
			}
		}

		if (const FMassUnitVisualFragment* VisualFrag = GetVisualFragment())
		{
			for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
			{
				if (Instance.TemplateISM == TemplateISM)
				{
					return Instance.BaseOffset.GetScale3D();
				}
			}
		}
	}

	if (!bUseSkeletalMovement && TemplateISM)
	{
		if (TemplateISM == ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			return InstanceXf.GetScale3D();
		}
		return TemplateISM->GetRelativeScale3D();
	}

	if (const USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		return SkelMesh->GetRelativeScale3D();
	}

	return GetActorScale3D();
}

void AMassUnitBase::ApplyLocalVisualScale(const FVector& NewLocalScale, UInstancedStaticMeshComponent* InISM)
{
	UInstancedStaticMeshComponent* TemplateISM = InISM ? InISM : ISMComponent;
	UInstancedStaticMeshComponent* TargetISM = nullptr;
	int32 TargetInstIndex = INDEX_NONE;

	// Prioritize Mass Manager Visuals
	if (GetMassVisualInstance(TemplateISM, TargetISM, TargetInstIndex))
	{
		FTransform InstanceXf;
		TargetISM->GetInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false);
		InstanceXf.SetScale3D(NewLocalScale);
		TargetISM->UpdateInstanceTransform(TargetInstIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
		return;
	}

	if (!bUseSkeletalMovement && TemplateISM)
	{
		if (TemplateISM == ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
		{
			FTransform InstanceXf;
			ISMComponent->GetInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false);
			InstanceXf.SetScale3D(NewLocalScale);
			ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceXf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ false);
			return;
		}

		TemplateISM->SetRelativeScale3D(NewLocalScale);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetRelativeScale3D(NewLocalScale);
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
		GetWorldTimerManager().ClearTimer(NiagaraRotateTimerHandle);
	}
}


void AMassUnitBase::MulticastPulsateISMScale_Implementation(const FVector& InMinScale, const FVector& InMaxScale, float TimeMinToMax, bool bEnable, UInstancedStaticMeshComponent* InISMComponent)
{
	if (HasAuthority())
	{
		if (bEnable) Rep_VE_ActiveEffects |= (1 << 0);
		else Rep_VE_ActiveEffects &= ~(1 << 0);
		Rep_VE_PulsateMinScale = InMinScale;
		Rep_VE_PulsateMaxScale = InMaxScale;
		Rep_VE_PulsateHalfPeriod = TimeMinToMax;
	}

	if (FMassVisualEffectFragment* EffectFrag = GetMutableEffectFragment())
	{
		EffectFrag->bPulsateEnabled = bEnable;
		EffectFrag->PulsateMinScale = InMinScale;
		EffectFrag->PulsateMaxScale = InMaxScale;
		EffectFrag->PulsateHalfPeriod = TimeMinToMax;
		EffectFrag->PulsateElapsed = 0.f;
		EffectFrag->PulsateTargetISM = InISMComponent ? InISMComponent : ISMComponent;
	}
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

				// If target is a building, ensure we follow at a safe distance from its obstacle
				const bool bIsBuilding = Cast<ABuildingBase>(NewFollowTarget) != nullptr;
				if (bIsBuilding)
				{
					float TargetRadius = 300.f;
					bool bBoundsFound = false;
					if (NewFollowTarget->NavObstacleProxy)
					{
						const FBox ProxyBox = NewFollowTarget->NavObstacleProxy->GetComponentsBoundingBox(true);
						if (ProxyBox.IsValid)
						{
							TargetRadius = ProxyBox.GetExtent().Size2D();
							bBoundsFound = true;
						}
					}
					
					if (!bBoundsFound)
					{
						const FBox B = NewFollowTarget->GetComponentsBoundingBox(true);
						if (B.IsValid)
						{
							TargetRadius = B.GetExtent().Size2D();
							bBoundsFound = true;
						}
					}

					if (!bBoundsFound)
					{
						if (const FMassAgentCharacteristicsFragment* TargetChar = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetHandle))
						{
							TargetRadius = TargetChar->CapsuleRadius;
						}
					}
					
					AITFrag->FollowRadius = TargetRadius + 150.f; // Substantial buffer to stay clear of dirty area
				}
				else
				{
					AITFrag->FollowRadius = 200.f; // Default for regular units
				}
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
		bool bWantsRepair = false;
		if (ThisUnit->IsWorker && ThisUnit->CanRepair && NewFollowTarget && NewFollowTarget->CanBeRepaired)
		{
			if (NewFollowTarget->Attributes && NewFollowTarget->Attributes->GetHealth() < NewFollowTarget->Attributes->GetMaxHealth())
			{
				bWantsRepair = true;
			}
		}

		if (bWantsRepair)
		{
			// Enter dedicated GoToRepair flow; specialized processors will drive follow updates
			Signals->SignalEntity(UnitSignals::GoToRepair, MassEntityHandle);
		}
		else if (NewFollowTarget)
		{
			if (ThisUnit->IsWorker)
			{
				if (ABuildingBase* Building = Cast<ABuildingBase>(NewFollowTarget))
				{
					if (Building->IsBase)
					{
						Signals->SignalEntity(UnitSignals::GoToBase, MassEntityHandle);
						return;
					}
				}
			}
			
			Signals->SignalEntity(UnitSignals::Run, MassEntityHandle);
			/*
			// Immediately push a MoveTarget towards the follow target's current location to get moving this tick.
			if (FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle))
			{
				const FVector NewTargetLocation = NewFollowTarget->GetMassActorLocation();
				float DesiredSpeed = 300.f;
				if (ThisUnit && ThisUnit->Attributes)
				{
					DesiredSpeed = ThisUnit->Attributes->GetBaseRunSpeed();
				}
				UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
			}
			*/
		}
		else
		{
			// Clearing follow: controllers will drive the next state/order; no signal here.
		}
	}
}
