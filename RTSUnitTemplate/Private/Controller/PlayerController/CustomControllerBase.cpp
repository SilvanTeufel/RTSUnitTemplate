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
void ACustomControllerBase::CorrectSetUnitMoveTarget(UObject* WorldContextObject, FMassEntityHandle InEntity, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius)
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

    // --- Access the PER-ENTITY fragment ---
    FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(InEntity);

    if (!MoveTargetFragmentPtr)
    {
        // If the entity doesn't have the fragment yet, you might need to add it.
        // Depending on your setup, it might be added by an Archetype or Trait already.
        // If you need to add it manually:
        // EntityManager.AddFragmentData<FMassMoveTargetFragment>(InEntity);
        // MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(InEntity);

        // Alternatively, if it's an error that it's missing:
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *InEntity.DebugGetDescription());
        return;
    }

    // Now, modify the specific entity's fragment data
    MoveTargetFragmentPtr->Center = NewTargetLocation;
    MoveTargetFragmentPtr->IntentAtGoal = EMassMovementAction::Move; // Set the intended action
    MoveTargetFragmentPtr->DesiredSpeed.Set(DesiredSpeed);
    MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
    MoveTargetFragmentPtr->Forward = FVector::ForwardVector; // Or calculate based on direction to target if needed
    
    // If you need to trigger network replication or specific actions:
    MoveTargetFragmentPtr->CreateNewAction(EMassMovementAction::Move, *World); // Resets action state, marks dirty
    // MoveTargetFragmentPtr->MarkNetDirty(); // If CreateNewAction doesn't do it implicitly
	// Inside CorrectSetUnitMoveTarget, after CreateNewAction
	if (MoveTargetFragmentPtr) // Check ptr again just in case
	{
		UE_LOG(LogTemp, Log, TEXT("SetUnitMoveTarget for Entity %s: Action triggered. CurrentAction is now: %s"),
			   *InEntity.DebugGetDescription(),
			   *UEnum::GetValueAsString(MoveTargetFragmentPtr->GetCurrentAction()));
	}
	
    UE_LOG(LogTemp, Log, TEXT("SetUnitMoveTarget: Updated move target for entity %s"), *InEntity.DebugGetDescription());
}

void ACustomControllerBase::RightClickPressedMass()
{
	
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit);
	
	if (!CheckClickOnTransportUnit(Hit))
	{
		if(!SelectedUnits.Num() || !SelectedUnits[0]->CurrentDraggedWorkArea)
		{
			GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
			if(!CheckClickOnWorkArea(Hit))
			{
				RunUnitsAndSetWaypointsMass(Hit);
			}
		}
	}


	if (SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
	}
	
}


void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
	int32 NumUnits = SelectedUnits.Num();
	//int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)NumUnits));
	const int32 GridSize = ComputeGridSize(NumUnits);
	AWaypoint* BWaypoint = nullptr;

	bool PlayWaypointSound = false;
	bool PlayRunSound = false;
	
	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		if (SelectedUnits[i] != CameraUnitWithTag)
		if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead) {
			
			//FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
			int32 Row = i / GridSize;     // Row index
			int32 Col = i % GridSize;     // Column index

			//FVector RunLocation = Hit.Location + FVector(Col * 100, Row * 100, 0.f);  // Adjust x and y positions equally for a square grid
			FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);

			RunLocation = TraceRunLocation(RunLocation);

			float Speed = SelectedUnits[i]->Attributes->GetBaseRunSpeed();
			FMassEntityHandle MassEntityHandle =  SelectedUnits[i]->MassActorBindingComponent->GetMassEntityHandle();
			
			UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Speed : %f"), Speed);
			if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
			{
				//UE_LOG(LogTemp, Warning, TEXT("This is a Building!!"));
				//PlayWaypointSound = true;
			}else if (IsShiftPressed) {
				//UE_LOG(LogTemp, Warning, TEXT("IsShiftPressed!"));
	
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);

				PlayRunSound = true;
			}else if(UseUnrealEnginePathFinding)
			{
				//UE_LOG(LogTemp, Warning, TEXT("MOVVVEE!"));
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);
				PlayRunSound = true;
			}
			else {
				//UE_LOG(LogTemp, Warning, TEXT("DIJKSTRA!"));
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);
				PlayRunSound = true;
			}
		}
	}

	if (WaypointSound && PlayWaypointSound)
	{
		UGameplayStatics::PlaySound2D(this, WaypointSound);
	}

	if (RunSound && PlayRunSound)
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastRunSoundTime >= RunSoundDelayTime) // Check if 3 seconds have passed
		{
			UGameplayStatics::PlaySound2D(this, RunSound);
			LastRunSoundTime = CurrentTime; // Update the timestamp
		}
	}
}
