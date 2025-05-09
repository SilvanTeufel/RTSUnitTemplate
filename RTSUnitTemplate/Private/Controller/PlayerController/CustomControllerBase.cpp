// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/CustomControllerBase.h"

#include "Landscape.h"
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
	if (IsValid(Unit))
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


void ACustomControllerBase::CorrectSetUnitMoveTarget_Implementation(UObject* WorldContextObject, FMassEntityHandle InEntity, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius)
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
	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(InEntity);
	
    if (!MoveTargetFragmentPtr || !AiStatePtr)
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

	AiStatePtr->StoredLocation = NewTargetLocation;
    // Now, modify the specific entity's fragment data
    MoveTargetFragmentPtr->Center = NewTargetLocation;
    MoveTargetFragmentPtr->IntentAtGoal = EMassMovementAction::Move; // Set the intended action
    MoveTargetFragmentPtr->DesiredSpeed.Set(DesiredSpeed);
    MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
    MoveTargetFragmentPtr->Forward = FVector::ForwardVector; // Or calculate based on direction to target if needed
    
    // If you need to trigger network replication or specific actions:
    MoveTargetFragmentPtr->CreateNewAction(EMassMovementAction::Move, *World); // Resets action state, marks dirty

	EntityManager.Defer().AddTag<FMassStateRunTag>(InEntity);

	if (AttackToggled)
	{
		//UE_LOG(LogTemp, Log, TEXT("ADDED DETECTION!"));
		EntityManager.Defer().AddTag<FMassStateDetectTag>(InEntity);
	}else
	{
		//UE_LOG(LogTemp, Log, TEXT("REMOVED DETECTION!"));
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(InEntity);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateDeadTag>(InEntity); 
	EntityManager.Defer().RemoveTag<FMassStateRunTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(InEntity);

	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(InEntity);
	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(InEntity);
	// MoveTargetFragmentPtr->MarkNetDirty(); // If CreateNewAction doesn't do it implicitly
	// Inside CorrectSetUnitMoveTarget, after CreateNewAction
	/*
	if (MoveTargetFragmentPtr) // Check ptr again just in case
	{
		UE_LOG(LogTemp, Log, TEXT("SetUnitMoveTarget for Entity %s: Action triggered. CurrentAction is now: %s"),
			   *InEntity.DebugGetDescription(),
			   *UEnum::GetValueAsString(MoveTargetFragmentPtr->GetCurrentAction()));
	}
	*/
    //E_LOG(LogTemp, Log, TEXT("SetUnitMoveTarget: Updated move target for entity %s"), *InEntity.DebugGetDescription());
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
			
			//UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Speed : %f"), Speed);
			if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
			{
				//UE_LOG(LogTemp, Warning, TEXT("This is a Building!!"));
				//PlayWaypointSound = true;
			}else if (IsShiftPressed) {
				//UE_LOG(LogTemp, Warning, TEXT("IsShiftPressed!"));
	
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);
				//SelectedUnits[i]->SetUnitState(UnitData::Run);
				SetUnitState_Replication(SelectedUnits[i], 1);
				PlayRunSound = true;
			}else if(UseUnrealEnginePathFinding)
			{
				//UE_LOG(LogTemp, Warning, TEXT("MOVVVEE!"));
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);
				//SelectedUnits[i]->SetUnitState(UnitData::Run);
				SetUnitState_Replication(SelectedUnits[i], 1);
				PlayRunSound = true;
			}
			else {
				//UE_LOG(LogTemp, Warning, TEXT("DIJKSTRA!"));
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);
				//SelectedUnits[i]->SetUnitState(UnitData::Run);
				SetUnitState_Replication(SelectedUnits[i], 1);
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


void ACustomControllerBase::LeftClickPressedMass()
{
	LeftClickIsPressed = true;
	AbilityArrayIndex = 0;
	
	if (!CameraBase || CameraBase->TabToggled) return;
	
	if(AltIsPressed)
	{
		DestroyWorkArea();
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			CancelAbilitiesIfNoBuilding(SelectedUnits[i]);
		}
		
	}else if (AttackToggled) {
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

		int32 NumUnits = SelectedUnits.Num();
		const int32 GridSize = ComputeGridSize(NumUnits);
		AWaypoint* BWaypoint = nullptr;

		bool PlayWaypointSound = false;
		bool PlayAttackSound = false;
		
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			if (SelectedUnits[i] != CameraUnitWithTag)
			{
				int32 Row = i / GridSize;     // Row index
				int32 Col = i % GridSize;     // Column index
				
				FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);
				RunLocation = TraceRunLocation(RunLocation);
				
				if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
				{
					// Do Nothing
				}else
				{
					DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
					LeftClickAttackMass(SelectedUnits[i], RunLocation);
					PlayAttackSound = true;
				}
			}
			
			if (SelectedUnits[i])
				FireAbilityMouseHit(SelectedUnits[i], Hit);
		}
		
		AttackToggled = false;
		
		if (WaypointSound && PlayWaypointSound)
		{
			UGameplayStatics::PlaySound2D(this, WaypointSound);
		}

		if (AttackSound && PlayAttackSound)
		{
			UGameplayStatics::PlaySound2D(this, AttackSound);
		}
		
	}
	else {
		DropWorkArea();
		//LeftClickSelect_Implementation();

		
		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);

		bool AbilityFired = false;
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
				if (SelectedUnits[i] && SelectedUnits[i]->CurrentSnapshot.AbilityClass && SelectedUnits[i]->CurrentDraggedAbilityIndicator)
				{
						FireAbilityMouseHit(SelectedUnits[i], Hit_Pawn);
						AbilityFired = true;
				}
		}
		
		if (AbilityFired) return;
		
		if (Hit_Pawn.bBlockingHit && HUDBase)
		{
			AActor* HitActor = Hit_Pawn.GetActor();
			
			if(!HitActor->IsA(ALandscape::StaticClass()))
				ClickedActor = Hit_Pawn.GetActor();
			else
				ClickedActor = nullptr;
			
			AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
			const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Hit_Pawn.GetActor());
			
			if (UnitBase && (UnitBase->TeamId == SelectableTeamId || SelectableTeamId == 0) && !SUnit )
			{
				HUDBase->DeselectAllUnits();
				HUDBase->SetUnitSelected(UnitBase);
				DragUnitBase(UnitBase);
		
				
				if(CameraBase->AutoLockOnSelect)
					LockCameraToUnit = true;
			}
			else {
				HUDBase->InitialPoint = HUDBase->GetMousePos2D();
				HUDBase->bSelectFriendly = true;
			}
		}
	}
	
}

void ACustomControllerBase::Server_ReportUnitVisibility_Implementation(APerformanceUnit* Unit, bool bVisible)
{
	if (IsValid(Unit))
	{
		Unit->SetClientVisibility(bVisible);
	}
}

void ACustomControllerBase::LeftClickAttackMass_Implementation(AUnitBase* Unit, FVector Location)
{
	if (Unit && Unit->UnitState != UnitData::Dead) {
	
		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);

		if (Hit_Pawn.bBlockingHit)
		{
			AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
					
			if(UnitBase && !UnitBase->TeamId)
			{
				/// Focus Enemy Units ///
				Unit->UnitToChase = UnitBase;
				SetUnitState_Replication(Unit, 3);
				
			}else if(UseUnrealEnginePathFinding)
			{
					
				if (Unit && Unit->UnitState != UnitData::Dead)
				{
					LeftClickAMoveUEPFMass(Unit, Location);
				}
					
			}else
			{
				LeftClickAMove(Unit, Location);
			}
		}else if(UseUnrealEnginePathFinding)
		{
			if (Unit && Unit->UnitState != UnitData::Dead)
			{
				/// A-Move Units ///
				LeftClickAMoveUEPFMass(Unit, Location);
			}
					
		}
			
	}
}

void ACustomControllerBase::LeftClickAMoveUEPFMass_Implementation(AUnitBase* Unit, FVector Location)
{
	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}

	float Speed = Unit->Attributes->GetBaseRunSpeed();
	FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
	SetUnitState_Replication(Unit,1);
	CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, Location, Speed, 40.f);

	//Unit->SetUnitState(UnitData::Run);
	//SetUnitState_Multi(Unit, 1);
	//MoveToLocationUEPathFinding(Unit, Location);
}

/*
CorrectSetUnitMoveTarget(GetWorld(), MassEntityHandle, RunLocation, Speed, 40.f);
SelectedUnits[i]->SetUnitState(UnitData::Run);
*/