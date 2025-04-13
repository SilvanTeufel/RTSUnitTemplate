// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/CustomControllerBase.h"

#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"

#include "MassEntitySubsystem.h"     // Needed for FMassEntityManager, UMassEntitySubsystem
#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassMovementFragments.h"  // Needed for EMassMovementAction, FMassVelocityFragment
#include "MassExecutor.h"          // Provides Defer() method context typically
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "Engine/World.h"
#include "Engine/Engine.h"


void ACustomControllerBase::Multi_SetFogManager_Implementation(const TArray<AActor*>& AllUnits)
{
	if (!IsLocalController()) return;
	
	// TSGameMode->AllUnits is onyl available on Server why we start running on server
	// We Mutlicast to CLient and send him AllUnits as parameter
	TArray<AUnitBase*> NewSelection;
	for (int32 i = 0; i < AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(AllUnits[i]);
		// Every Controller can Check his own TeamId
		if (Unit && Unit->GetUnitState() != UnitData::Dead && Unit->TeamId == SelectableTeamId)
		{
			NewSelection.Add(Unit);
		}else if (Unit && Unit->SpawnedFogManager)
		{
			Unit->DestroyFogManager();
		}
	}

	// And we can create the FogManager
	for (int32 i = 0; i < NewSelection.Num(); i++)
	{
		if (NewSelection[i] && NewSelection[i]->TeamId == SelectableTeamId)
		{
			NewSelection[i]->IsMyTeam = true;
			NewSelection[i]->SpawnFogOfWarManager(this);
		}
	}

	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetPawn());
	if (Camera)
		Camera->SetupResourceWidget(this);
}


void ACustomControllerBase::Multi_SetFogManagerUnit_Implementation(APerformanceUnit* Unit)
{
	if (Unit->TeamId == SelectableTeamId)
	{
		Unit->IsMyTeam = true;
		Unit->SpawnFogOfWarManager(this);
	}
}

void ACustomControllerBase::Multi_ShowWidgetsWhenLocallyControlled_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("Multi_HideWidgetWhenNoControl_Implementation - TeamId is now: %d"), SelectableTeamId);
	
	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(CameraBase);
	if (Camera)
		Camera->ShowWidgetsWhenLocallyControlled();
}

void ACustomControllerBase::Multi_SetCamLocation_Implementation(FVector NewLocation)
{
	//if (!IsLocalController()) return;
	UE_LOG(LogTemp, Log, TEXT("Multi_HideWidgetWhenNoControl_Implementation - TeamId is now: %d"), SelectableTeamId);
	
	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(CameraBase);
	if (Camera)
		Camera->SetActorLocation(NewLocation);
}

void ACustomControllerBase::Multi_HideEnemyWaypoints_Implementation()
{
	// Only execute for the local controller
	if (!IsLocalController())
	{
		return;
	}
	//if (!IsLocalController()) return;
	UE_LOG(LogTemp, Log, TEXT("Multi_HideEnemyWaypoints_Implementation - TeamId is now: %d"), SelectableTeamId);
	// Try to get all AWaypoints* Waypoint with TArray from Map
	// Compare Waypoint->TeamId with SelectableTeamId (from this Class)
	// If Unequal please hide
	// If TeamId == 0 dont hide

	// Retrieve all waypoints from the world
	TArray<AActor*> FoundWaypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), FoundWaypoints);

	// Iterate over each waypoint
	for (AActor* Actor : FoundWaypoints)
	{
		if (AWaypoint* Waypoint = Cast<AWaypoint>(Actor))
		{
			// Hide the waypoint if its TeamId is different from our team AND it isn't neutral (TeamId == 0)
			if (Waypoint->TeamId != SelectableTeamId && Waypoint->TeamId != 0)
			{
				Waypoint->SetActorHiddenInGame(true);
			}
		}
	}
}


void ACustomControllerBase::AgentInit_Implementation()
{
	// Only execute for the local controller
	if (!IsLocalController())
	{
		return;
	}
	if (GetWorld() && GetWorld()->IsNetMode(ENetMode::NM_Client))
	{
		UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!!ACustomControllerBase!!!!!!!!!!AgentInitialization on Client!!!!!!!!!!!!!!!!!!!!!!!!"));
	}else
	{
		UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!ACustomControllerBase!!!!!!!!!!!AgentInitialization on Server!!!!!!!!!!!!!!!!!!!!!!!!"));
	}
	
	
	ARLAgent* Camera = Cast<ARLAgent>(CameraBase);
	if (Camera)
		Camera->AgentInitialization();
}


/**
 * Gives a move command to a specific Mass entity by adding or updating its FMassMoveTargetFragment.
 * Uses deferred commands for safety. Should be placed in an appropriate manager class, controller, or BPL.
 *
 * @param WorldContextObject Context object to get the World.
 * @param InEntity The handle of the entity to command.
 * @param NewTargetLocation The destination location vector.
 * @param DesiredSpeed The speed at which the entity should move towards the target.
 * @param AcceptanceRadius The distance from the target at which the entity should stop.
 */
/*
void  ACustomControllerBase::CorrectSetUnitMoveTarget(UObject* WorldContextObject, FMassEntityHandle InEntity, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: WorldContextObject is invalid or could not provide World."));
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: MassEntitySubsystem not found. Is Mass enabled?"));
		return;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	if (!EntityManager.IsEntityValid(InEntity))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is invalid."), *InEntity.DebugGetDescription());
		return;
	}

	// Define a hash key for the shared fragment.
	// Use a suitable hash value. Here it's hardcoded,
	// but in a real use-case the hash could be computed based on some criteria.
	constexpr uint32 SharedFragmentHash = 0x12345678;

	// Retrieve (or create) the shared fragment as a const reference.
	const FSharedStruct& SharedFragmentStructConst = EntityManager.GetOrCreateSharedFragment<FMassMoveTargetFragment>(SharedFragmentHash);


	// Remove the const to allow modification.
	FSharedStruct& SharedFragmentStruct = const_cast<FSharedStruct&>(SharedFragmentStructConst);

	// Using Get<T> to obtain a mutable reference to the FMassMoveTargetFragment.
	FMassMoveTargetFragment& SharedFragment = SharedFragmentStruct.Get<FMassMoveTargetFragment>();

	
	// Update the shared fragment's data.
	SharedFragment.Center = NewTargetLocation;
	SharedFragment.IntentAtGoal = EMassMovementAction::Move;
	SharedFragment.DesiredSpeed.Set(DesiredSpeed);
	SharedFragment.SlackRadius = AcceptanceRadius;

	// Optionally, you might log that the shared target was updated.
	// Note: All entities that reference this shared fragment with the same hash will see these new values.
	UE_LOG(LogTemp, Log, TEXT("CorrectSetUnitMoveTarget: Updated shared move target for entity %s"), *InEntity.DebugGetDescription());
}
*/