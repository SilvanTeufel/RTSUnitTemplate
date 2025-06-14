// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/CustomControllerBase.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"

#include "MassEntitySubsystem.h"     // Needed for FMassEntityManager, UMassEntitySubsystem
#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassMovementFragments.h"  // Needed for EMassMovementAction, FMassVelocityFragment
#include "MassExecutor.h"          // Provides Defer() method context typically
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "Actors/FogActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Mass/Signals/MySignals.h"


void ACustomControllerBase::Multi_SetMyTeamUnits_Implementation(const TArray<AActor*>& AllUnits)
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
		}
	}

	// And we can create the FogManager
	for (int32 i = 0; i < NewSelection.Num(); i++)
	{
		if (NewSelection[i] && NewSelection[i]->TeamId == SelectableTeamId)
		{
			NewSelection[i]->IsMyTeam = true;
			//NewSelection[i]->OwningPlayerController = this;
			//NewSelection[i]->SetOwningPlayerController();
			//NewSelection[i]->SpawnFogOfWarManager(this);
		}
	}

	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetPawn());
	if (Camera)
		Camera->SetupResourceWidget(this);
}

/*
void ACustomControllerBase::Multi_SetFogManagerUnit_Implementation(APerformanceUnit* Unit)
{
	if (IsValid(Unit))
		if (Unit->TeamId == SelectableTeamId)
		{
			Unit->IsMyTeam = true;
			Unit->SpawnFogOfWarManager(this);
		}
}*/

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
		//UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!!ACustomControllerBase!!!!!!!!!!AgentInitialization on Client!!!!!!!!!!!!!!!!!!!!!!!!"));
	}else
	{
		//UE_LOG(LogTemp, Error, TEXT("!!!!!!!!!ACustomControllerBase!!!!!!!!!!!AgentInitialization on Server!!!!!!!!!!!!!!!!!!!!!!!!"));
	}
	
	
	ARLAgent* Camera = Cast<ARLAgent>(CameraBase);
	if (Camera)
		Camera->AgentInitialization();
}


void ACustomControllerBase::CorrectSetUnitMoveTarget_Implementation(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius, bool AttackT)
{
	if (!Unit) return;

	if (!Unit->IsInitialized) return;
	if (!Unit->CanMove) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{

		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}
	
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

	FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
    if (!EntityManager.IsEntityValid(MassEntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is invalid."), *MassEntityHandle.DebugGetDescription());
        return;
    }

    // --- Access the PER-ENTITY fragment ---
    FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	
    if (!MoveTargetFragmentPtr || !AiStatePtr)
    {
        // If the entity doesn't have the fragment yet, you might need to add it.
        // Depending on your setup, it might be added by an Archetype or Trait already.
        // If you need to add it manually:
        // EntityManager.AddFragmentData<FMassMoveTargetFragment>(InEntity);
        // MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(InEntity);

        // Alternatively, if it's an error that it's missing:
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
        return;
    }

	AiStatePtr->StoredLocation = NewTargetLocation;
	AiStatePtr->PlaceholderSignal = UnitSignals::Run;
    // Now, modify the specific entity's fragment data
	/*
    MoveTargetFragmentPtr->Center = NewTargetLocation;
    MoveTargetFragmentPtr->IntentAtGoal = EMassMovementAction::Move; // Set the intended action
    MoveTargetFragmentPtr->DesiredSpeed.Set(DesiredSpeed);
    MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
    MoveTargetFragmentPtr->Forward = FVector::ForwardVector; // Or calculate based on direction to target if needed
    
    // If you need to trigger network replication or specific actions:
    MoveTargetFragmentPtr->CreateNewAction(EMassMovementAction::Move, *World); // Resets action state, marks dirty
	*/
	UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
	
	EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
	if (AttackT)
	{
		EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
	}else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateDeadTag>(MassEntityHandle); 
	//EntityManager.Defer().RemoveTag<FMassStateRunTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);

	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
}


void ACustomControllerBase::LoadUnitsMass_Implementation(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter)
{
		if (Transporter && Transporter->IsATransporter) // Transporter->IsATransporter
		{

			// Set up start and end points for the line trace (downward direction)
			FVector Start = Transporter->GetActorLocation();
			FVector End = Start - FVector(0.f, 0.f, 10000.f); // Trace far enough downwards

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			// Ignore the transporter itself
			QueryParams.AddIgnoredActor(Transporter);

			// Perform the line trace on a suitable collision channel, e.g., ECC_Visibility or a custom one
			bool DidHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
			
			
			for (int32 i = 0; i < UnitsToLoad.Num(); i++)
			{
				if (UnitsToLoad[i] && UnitsToLoad[i]->UnitState != UnitData::Dead && UnitsToLoad[i]->CanBeTransported)
				{
					// Calculate the distance between the selected unit and the transport unit in X/Y space only.
					float Distance = FVector::Dist2D(UnitsToLoad[i]->GetActorLocation(), Transporter->GetActorLocation());

					// If the unit is within 250 units, load it instantly.
					if (Distance <= Transporter->InstantLoadRange)
					{
						Transporter->LoadUnit(UnitsToLoad[i]);
					}
					else
					{
						// Otherwise, set it as ready for transport so it can move towards the transporter.
						UnitsToLoad[i]->SetRdyForTransport(true);
					}
					// Perform the line trace on a suitable collision channel, e.g., ECC_Visibility or a custom one
					if (DidHit)
					{
						// Use the hit location's Z coordinate and keep X and Y from the transporter
						FVector NewRunLocation = Transporter->GetActorLocation();
						NewRunLocation.Z = HitResult.Location.Z+50.f;
						UnitsToLoad[i]->RunLocation = NewRunLocation;
					}
					else
					{
						// Fallback: if no hit, subtract a default fly height
						UnitsToLoad[i]->RunLocation = Transporter->GetActorLocation();
					}
					
					if (SelectedUnits[i]->MassActorBindingComponent->bIsMassUnit)
					{
						float Speed = SelectedUnits[i]->Attributes->GetBaseRunSpeed();
						CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], UnitsToLoad[i]->RunLocation, Speed, 40.f);
						SetUnitState_Replication(SelectedUnits[i], 1);
					}
					else
					{
						RightClickRunUEPF(UnitsToLoad[i], UnitsToLoad[i]->RunLocation, true);
					}
				}
			}
			
			SetUnitState_Replication(Transporter,0);

		}else
		{
			for (int32 i = 0; i < UnitsToLoad.Num(); i++)
			{
				if (UnitsToLoad[i] && UnitsToLoad[i]->UnitState != UnitData::Dead && UnitsToLoad[i]->CanBeTransported)
				{
					UnitsToLoad[i]->SetRdyForTransport(false);
				}
			}
		}
	
}

bool ACustomControllerBase::CheckClickOnTransportUnitMass(FHitResult Hit_Pawn)
{
	if (!Hit_Pawn.bBlockingHit) return false;


		AActor* HitActor = Hit_Pawn.GetActor();
		
		AUnitBase* UnitBase = Cast<AUnitBase>(HitActor);
	
		LoadUnitsMass(SelectedUnits, UnitBase);
	
		if (UnitBase && UnitBase->IsATransporter){
		
			TArray<AUnitBase*> NewSelection;

			NewSelection.Emplace(UnitBase);
			
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this, NewSelection]()
				{
					Client_UpdateHUDSelection(NewSelection, SelectableTeamId);
				},
				1.0f,  // Delay time in seconds (change as needed)
				false  // Do not loop
			);
			return true;
		}
	return false;
}

void ACustomControllerBase::RightClickPressedMass()
{
	
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit);
	
	if (!CheckClickOnTransportUnitMass(Hit))
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
		
			
			//UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Speed : %f"), Speed);
			if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
			{
				//UE_LOG(LogTemp, Warning, TEXT("This is a Building!!"));
				//PlayWaypointSound = true;
			}else if (IsShiftPressed) {
				//UE_LOG(LogTemp, Warning, TEXT("IsShiftPressed!"));
				
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);

				if (!SelectedUnits[i]->IsInitialized) return;
				if (!SelectedUnits[i]->CanMove) return;
				
				if (SelectedUnits[i]->MassActorBindingComponent->bIsMassUnit)
				{
					CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], RunLocation, Speed, 40.f);
					SetUnitState_Replication(SelectedUnits[i], 1);
				}
				else
				{
					RightClickRunShift(SelectedUnits[i], RunLocation);
				}
				
				PlayRunSound = true;
			}else if(UseUnrealEnginePathFinding)
			{
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);

				if (!SelectedUnits[i]->IsInitialized) return;
				if (!SelectedUnits[i]->CanMove) return;
				
				if (SelectedUnits[i]->MassActorBindingComponent->bIsMassUnit)
				{
					CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], RunLocation, Speed, 40.f);
					SetUnitState_Replication(SelectedUnits[i], 1);
				}
				else
				{
					RightClickRunUEPF(SelectedUnits[i], RunLocation, true);
				}
				
				PlayRunSound = true;
			}
			else {
				//UE_LOG(LogTemp, Warning, TEXT("DIJKSTRA!"));
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);

				if (!SelectedUnits[i]->IsInitialized) return;
				if (!SelectedUnits[i]->CanMove) return;
				
				if (SelectedUnits[i]->MassActorBindingComponent->bIsMassUnit)
				{
					CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], RunLocation, Speed, 40.f);
					SetUnitState_Replication(SelectedUnits[i], 1);
				}
				else
					RightClickRunDijkstraPF(SelectedUnits[i], RunLocation, i);
				
				
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
					if (SelectedUnits[i]->MassActorBindingComponent->bIsMassUnit)
					{
						LeftClickAttackMass(SelectedUnits[i], RunLocation, AttackToggled);
					}else
					{
						LeftClickAttack(SelectedUnits[i], RunLocation);
					}
					
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
		bool AbilityUnSynced = false;
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
				if (SelectedUnits[i] && SelectedUnits[i]->CurrentSnapshot.AbilityClass && SelectedUnits[i]->CurrentDraggedAbilityIndicator)
				{
						FireAbilityMouseHit(SelectedUnits[i], Hit_Pawn);
						AbilityFired = true;
				}else
				{
					AbilityUnSynced = true;
				}
		}
		
		if (AbilityFired && !AbilityUnSynced) return;
		
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

void ACustomControllerBase::LeftClickAttackMass_Implementation(AUnitBase* Unit, FVector Location, bool AttackT)
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
					LeftClickAMoveUEPFMass(Unit, Location, AttackT);
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
				LeftClickAMoveUEPFMass(Unit, Location, AttackT);
			}
					
		}
			
	}
}

void ACustomControllerBase::LeftClickAMoveUEPFMass_Implementation(AUnitBase* Unit, FVector Location, bool AttackT)
{
	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}

	float Speed = Unit->Attributes->GetBaseRunSpeed();
	//FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
	SetUnitState_Replication(Unit,1);
	CorrectSetUnitMoveTarget(GetWorld(), Unit, Location, Speed, 40.f, AttackT);

	//Unit->SetUnitState(UnitData::Run);
	//SetUnitState_Multi(Unit, 1);
	//MoveToLocationUEPathFinding(Unit, Location);
}

void ACustomControllerBase::UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities)
{
    UWorld* World = GetWorld();
    if (!ensure(World)) return;

    // Prepare three parallel arrays
    TArray<FVector_NetQuantize> Positions;
    TArray<float>               WorldRadii;
    TArray<uint8>               UnitTeamIds;

    if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
    {
        FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& E : Entities)
        {
            if (!EM.IsEntityValid(E))
                continue;

            const FTransformFragment*               TF       = EM.GetFragmentDataPtr<FTransformFragment>(E);
            const FMassCombatStatsFragment*         StateFrag= EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
            const FMassAIStateFragment*             AI       = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
            const FMassAgentCharacteristicsFragment* CharFrag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);

            if (!TF || !StateFrag || !AI || !CharFrag)
                continue;

            // skip dead/despawned
            if (StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime)
                continue;

                // 1) world‐loc
                Positions.Add(FVector_NetQuantize(TF->GetTransform().GetLocation()));
                // 2) world‐space sight radius
                WorldRadii.Add(StateFrag->SightRadius);
                // 3) unit’s team
                UnitTeamIds.Add(StateFrag->TeamId);
        }
    }

    // **One** multicast to all FogActors
    for (TActorIterator<AFogActor> It(World); It; ++It)
    {
        It->Multicast_UpdateFogMaskWithCircles(Positions, WorldRadii, UnitTeamIds);
    }
}

/*
 void ACustomControllerBase::UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AFogActor> It(World); It; ++It)
	{
		AFogActor* FogActor = *It;
		//UE_LOG(LogTemp, Warning, TEXT("FogActor->TeamId! %d"), FogActor->TeamId);
		//UE_LOG(LogTemp, Warning, TEXT("SelectableTeamId! %d"), SelectableTeamId);
		//if (FogActor && FogActor->TeamId == SelectableTeamId)
		{
			FogActor->Multicast_UpdateFogMaskWithCircles(Entities);
		}
	}
}
*/