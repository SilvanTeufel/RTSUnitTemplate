// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/ExtendedControllerBase.h"

#include "GameplayTagsManager.h"
#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameModes/ResourceGameMode.h"

void AExtendedControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	MoveDraggedUnit_Implementation(DeltaSeconds);
	MoveWorkArea_Implementation(DeltaSeconds);
}

void AExtendedControllerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AExtendedControllerBase, CurrentDraggedWorkArea);
}

void AExtendedControllerBase::ActivateAbilitiesByIndex_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID)
{
	if (!UnitBase)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ActivateAbilitiesByIndex AbilityArrayIndex: %d"), AbilityArrayIndex);
	switch (AbilityArrayIndex)
	{
	case 0:
		ActivateDefaultAbilities(UnitBase, InputID);
		break;
	case 1:
		ActivateSecondAbilities(UnitBase, InputID);
		break;
	case 2:
		ActivateThirdAbilities(UnitBase, InputID);
		break;
	case 3:
		ActivateFourthAbilities(UnitBase, InputID);
		break;
	default:
		// Optionally handle invalid indices
			break;
	}
}

void AExtendedControllerBase::ActivateDefaultAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID)
{
	if(UnitBase && UnitBase->DefaultAbilities.Num())
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->DefaultAbilities);
	}
	
}

void AExtendedControllerBase::ActivateSecondAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID)
{
	if (UnitBase && UnitBase->SecondAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->SecondAbilities);
	}
}

void AExtendedControllerBase::ActivateThirdAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID)
{
	if (UnitBase && UnitBase->ThirdAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->ThirdAbilities);
	}
}

void AExtendedControllerBase::ActivateFourthAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID)
{
	if (UnitBase && UnitBase->FourthAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->FourthAbilities);
	}
}

void AExtendedControllerBase::ActivateAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities)
{
	if (UnitBase && Abilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, Abilities);
	}
}

void AExtendedControllerBase::ActivateDefaultAbilitiesByTag(EGASAbilityInputID InputID, FGameplayTag Tag)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(FGameplayTagContainer(Tag)) && (Unit->TeamId == SelectableTeamId))
		{
			ActivateDefaultAbilities_Implementation(Unit, InputID);
		}
	}
}


void AExtendedControllerBase::AddAbilityIndex(int Add)
{

	int NewIndex = AbilityArrayIndex+=Add;
	
	if (NewIndex > MaxAbilityArrayIndex)
		AbilityArrayIndex = 0;
	else if (NewIndex < 0)
		AbilityArrayIndex = MaxAbilityArrayIndex;
	else
		AbilityArrayIndex=NewIndex;

	UE_LOG(LogTemp, Warning, TEXT("MaxAbilityArrayIndex: %d"), MaxAbilityArrayIndex);
	UE_LOG(LogTemp, Warning, TEXT("Final AbilityArrayIndex after wrap: %d"), AbilityArrayIndex);
}

void AExtendedControllerBase::ApplyMovementInputToUnit(const FVector& Direction, float Scale, AUnitBase* Unit)
{
	if (Unit && (Unit->TeamId == SelectableTeamId))
	{
		if (Unit->GetController()) // Ensure the unit has a valid Controller
		{
			Unit->AddMovementInput(Direction, Scale);
			Unit->SetUnitState(UnitData::Run);
		}
	}
}

void AExtendedControllerBase::GetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID)
{
		if(!RTSGameMode || !RTSGameMode->AllUnits.Num()) return;
	
		AUnitBase* ClosestUnit = nullptr;
		float ClosestDistanceSquared = FLT_MAX;
		// If we are on the server, run the logic directly.
		for (AActor* UnitActor  : RTSGameMode->AllUnits)
		{
			// Cast to AGASUnit to make sure it's of the correct type
			AUnitBase* Unit = Cast<AUnitBase>(UnitActor);
			// Check if the unit is valid and has the same TeamId as the camera

			if (Unit && Unit->IsWorker && Unit->TeamId == PlayerTeamId && !Unit->BuildArea) // && !Unit->BuildArea
			{
		
				float DistanceSquared = FVector::DistSquared(Position, Unit->GetActorLocation());
				// Check if this unit is closer than the currently tracked closest unit
				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestUnit = Unit;
				}
			}
		}
		
		if (ClosestUnit)
		{
	
			ClosestUnit->SetSelected();
			SelectedUnits.Emplace(ClosestUnit);
			ActivateDefaultAbilities_Implementation(ClosestUnit, InputID);
			//OnAbilityInputDetected(InputID, ClosestUnit, ClosestUnit->DefaultAbilities);
		}
}

void AExtendedControllerBase::ServerGetClosestUnitTo_Implementation(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID)
{

	if(!RTSGameMode || !RTSGameMode->AllUnits.Num()) return;
	
	AUnitBase* ClosestUnit = nullptr;
	float ClosestDistanceSquared = FLT_MAX;

	for (AActor* UnitActor : RTSGameMode->AllUnits)
	{
		// Cast to AUnitBase to make sure it's of the correct type
		AUnitBase* Unit = Cast<AUnitBase>(UnitActor);
		// Check if the unit is valid and has the same TeamId as the player
		if (Unit && Unit->IsWorker && Unit->TeamId == PlayerTeamId && !Unit->BuildArea) // && !Unit->BuildArea
		{
			float DistanceSquared = FVector::DistSquared(Position, Unit->GetActorLocation());
			// Check if this unit is closer than the currently tracked closest unit
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestUnit = Unit;
			}
		}
	}

	// After finding the closest unit, notify the client
	ClientReceiveClosestUnit(ClosestUnit, InputID);
	ActivateDefaultAbilities_Implementation(ClosestUnit, InputID);
}

void AExtendedControllerBase::ClientReceiveClosestUnit_Implementation(AUnitBase* ClosestUnit, EGASAbilityInputID InputID)
{

	if (ClosestUnit)
	{
		ClosestUnit->SetSelected();
		SelectedUnits.Emplace(ClosestUnit);
		HUDBase->SetUnitSelected(ClosestUnit);
	}
	// Update your local CloseUnit reference with ClosestUnit here.
	// Example:
	// CloseUnit = ClosestUnit;
}

void AExtendedControllerBase::ActivateKeyboardAbilitiesOnCloseUnits(EGASAbilityInputID InputID, FVector CameraLocation, int PlayerTeamId, AHUDBase* HUD)
{

	if (HasAuthority())
	{
		GetClosestUnitTo(CameraLocation, PlayerTeamId, InputID);
	}
	else
	{
		// If we are on the client, request it from the server.
		ServerGetClosestUnitTo(CameraLocation, PlayerTeamId, InputID);
	}
}

void AExtendedControllerBase::ActivateKeyboardAbilitiesOnMultipleUnits(EGASAbilityInputID InputID)
{
	if (SelectedUnits.Num() > 0)
	{
		for (AGASUnit* SelectedUnit : SelectedUnits)
		{
			if (SelectedUnit)
			{
				//ActivateDefaultAbilities(SelectedUnit, InputID);
				ActivateAbilitiesByIndex_Implementation(SelectedUnit, InputID);
			}
		}
	}else if(HUDBase)
	{
		FVector CameraLocation = GetPawn()->GetActorLocation();
		ActivateKeyboardAbilitiesOnCloseUnits(InputID, CameraLocation, SelectableTeamId, HUDBase);
	}
}

void AExtendedControllerBase::SetWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FVector NewActorPosition)
{
	if (!DraggedArea)
	{
		return; // Exit the function if DraggedArea is null
	}

	DraggedArea->SetActorLocation(NewActorPosition);
}


void AExtendedControllerBase::MoveWorkArea_Implementation(float DeltaSeconds)
{
	// Check if there's a CurrentDraggedWorkArea
	if(SelectedUnits.Num() && SelectedUnits[0])
	if (SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		FVector MousePosition, MouseDirection;
		DeprojectMousePositionToWorld(MousePosition, MouseDirection);

		// Raycast from the mouse position into the scene to find the ground
		FVector Start = MousePosition;
		FVector End = Start + MouseDirection * 5000.f; // Extend to a maximum reasonable distance

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing
		CollisionParams.AddIgnoredActor(SelectedUnits[0]->CurrentDraggedWorkArea); // Ignore the dragged actor in the raycast

		// Perform the raycast
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

		// Check if something was hit
		if (bHit && HitResult.GetActor() != nullptr)
		{
			CurrentDraggedGround = HitResult.GetActor();
			// Update the work area's position to the hit location
			FVector NewActorPosition = HitResult.Location;
			NewActorPosition.Z += 50.f; // Adjust the Z offset if needed
			SetWorkAreaPosition(SelectedUnits[0]->CurrentDraggedWorkArea, NewActorPosition);
		}
	}
}


void AExtendedControllerBase::SendWorkerToWork_Implementation(AUnitBase* Worker)
{

	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("Worker is null! Cannot proceed."));
		return;
	}

	if (!Worker->CurrentDraggedWorkArea)
	{
		UE_LOG(LogTemp, Error, TEXT("Worker->CurrentDraggedWorkArea is null! Cannot assign to Worker."));
		return;
	}
	
		Worker->BuildArea = Worker->CurrentDraggedWorkArea;
		Worker->BuildArea->TeamId = Worker->TeamId;
		Worker->BuildArea->PlannedBuilding = true;
		Worker->BuildArea->AddAreaToGroup();
		AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(RTSGameMode);

		if(ResourceGameMode && Worker->BuildArea->IsPaid)
		{
			ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, -Worker->BuildArea->ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, -Worker->BuildArea->ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.LegendaryCost);
		}
	
		// Check if the worker is overlapping with the build area
		if (Worker->IsOverlappingActor(Worker->BuildArea))
		{
			// If they are overlapping, set the state to 'Build'
			Worker->SetUnitState(UnitData::Build);
			Worker->SetUEPathfinding = true;
		}
		else
		{
			// If they are not overlapping, set the state to 'GoToBuild'
			Worker->SetUnitState(UnitData::GoToBuild);
			Worker->SetUEPathfinding = true;
		}
	

	

	
	 Worker->CurrentDraggedWorkArea = nullptr;
}


bool AExtendedControllerBase::DropWorkArea()
{
	if(SelectedUnits.Num() && SelectedUnits[0])
	if (SelectedUnits[0]->CurrentDraggedWorkArea && SelectedUnits[0]->CurrentDraggedWorkArea->PlannedBuilding == false)
	{
		// Get all actors overlapping with the CurrentDraggedWorkArea
		TArray<AActor*> OverlappingActors;
		SelectedUnits[0]->CurrentDraggedWorkArea->GetOverlappingActors(OverlappingActors);

		bool bIsOverlappingWithValidArea = false;

		// Loop through the overlapping actors to check if they are instances of AWorkArea or ABuildingBase
		for (AActor* OverlappedActor : OverlappingActors)
		{
			if (OverlappedActor->IsA(AWorkArea::StaticClass()) || OverlappedActor->IsA(ABuildingBase::StaticClass()))
			{
				bIsOverlappingWithValidArea = true;
				break;
			}
		}

		// If overlapping with AWorkArea or ABuildingBase, destroy the CurrentDraggedWorkArea
		if (bIsOverlappingWithValidArea)
		{
			SelectedUnits[0]->CurrentDraggedWorkArea->Destroy();
			return true;
		}
		
		if(SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->IsWorker)
		{
				SendWorkerToWork(SelectedUnits[0]);
				return true;
		}
		
		//CurrentDraggedWorkArea = nullptr;
	}
	return false;
}

void AExtendedControllerBase::MoveDraggedUnit_Implementation(float DeltaSeconds)
{
	// Optionally, attach the actor to the cursor
	if(CurrentDraggedUnitBase)
	{
		FVector MousePosition, MouseDirection;
		DeprojectMousePositionToWorld(MousePosition, MouseDirection);

		// Raycast from the mouse position into the scene to find the ground
		FVector Start = MousePosition;
		FVector End = Start + MouseDirection * 5000; // Extend to a maximum reasonable distance

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing
		CollisionParams.AddIgnoredActor(CurrentDraggedUnitBase); // Ignore the dragged actor in the raycast

		// Perform the raycast
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

		// Check if something was hit
		if (bHit && HitResult.GetActor() != nullptr)
		{
			CurrentDraggedGround = HitResult.GetActor();
			// Update the actor's position to the hit location
			FVector NewActorPosition = HitResult.Location;
			NewActorPosition.Z += 50.f;
			CurrentDraggedUnitBase->SetActorLocation(NewActorPosition);
		}

	}
}

void AExtendedControllerBase::DragUnitBase(AUnitBase* UnitToDrag)
{
	if(UnitToDrag->IsOnPlattform && SpawnPlatform)
	{
		if(SpawnPlatform->GetEnergy() >= UnitToDrag->EnergyCost)
		{
			CurrentDraggedUnitBase = UnitToDrag;
			CurrentDraggedUnitBase->IsDragged = true;
			SpawnPlatform->SetEnergy(SpawnPlatform->Energy-UnitToDrag->EnergyCost);
		}
	}
}

void AExtendedControllerBase::DropUnitBase()
{
	AUnitSpawnPlatform* CurrentSpawnPlatform = Cast<AUnitSpawnPlatform>(CurrentDraggedGround);
	if(CurrentDraggedUnitBase && CurrentDraggedUnitBase->IsOnPlattform && !CurrentSpawnPlatform && SpawnPlatform)
	{
		CurrentDraggedUnitBase->IsOnPlattform = false;
		CurrentDraggedUnitBase->IsDragged = false;
		CurrentDraggedUnitBase->SetUnitState(UnitData::PatrolRandom);
		CurrentDraggedUnitBase = nullptr;
	}else if(CurrentDraggedUnitBase && CurrentDraggedUnitBase->IsOnPlattform && SpawnPlatform)
	{
		SpawnPlatform->SetEnergy(SpawnPlatform->Energy+CurrentDraggedUnitBase->EnergyCost);
		CurrentDraggedUnitBase->IsDragged = false;
		CurrentDraggedUnitBase->SetUnitState(UnitData::Idle);
		CurrentDraggedUnitBase = nullptr;
	}
}
/*
void AExtendedControllerBase::DestoryWorkAreaOnServer_Implementation(AWorkArea* WorkArea)
{
    UE_LOG(LogTemp, Warning, TEXT("DestoryWorkAreaOnServer_Implementation called on %s"), HasAuthority() ? TEXT("Server") : TEXT("Client"));

    if (WorkArea)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server: Attempting to destroy WorkArea: %s"), *WorkArea->GetName());
        WorkArea->Destroy();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Server: WorkArea is nullptr, nothing to destroy."));
    }
}

void AExtendedControllerBase::DestoryWorkArea()
{
    UE_LOG(LogTemp, Warning, TEXT("DestoryWorkArea called on %s"), HasAuthority() ? TEXT("Server") : TEXT("Client"));

    FHitResult HitResult;
    bool bHitUnderCursor = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

    UE_LOG(LogTemp, Warning, TEXT("GetHitResultUnderCursor returned: %s"), bHitUnderCursor ? TEXT("True") : TEXT("False"));

    if (bHitUnderCursor)
    {
        AActor* HitActor = HitResult.GetActor();
        if (HitActor)
        {
            UE_LOG(LogTemp, Warning, TEXT("Hit Actor: %s"), *HitActor->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("HitResult Actor is nullptr."));
        }

        AWorkArea* WorkArea = Cast<AWorkArea>(HitActor);
        if (WorkArea)
        {
            UE_LOG(LogTemp, Warning, TEXT("Hit Actor is a WorkArea with Type: %d and TeamId: %d"), (int32)WorkArea->Type, WorkArea->TeamId);

            if (WorkArea->Type == WorkAreaData::BuildArea && WorkArea->TeamId == SelectableTeamId)
            {
                UE_LOG(LogTemp, Warning, TEXT("WorkArea is a valid BuildArea for this team. Calling server to destroy."));
                DestoryWorkAreaOnServer(WorkArea);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("WorkArea type or team does not match. Not destroying."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Hit Actor is not a WorkArea."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid hit under cursor to destroy WorkArea."));
    }
}
*/

void AExtendedControllerBase::DestoryWorkAreaOnServer_Implementation(AWorkArea* WorkArea)
{
	if (WorkArea) WorkArea->Destroy();;
}

void AExtendedControllerBase::DestoryWorkArea()
{
	// Use Hit (FHitResult) and GetHitResultUnderCursor
	// If you hit AWorkArea with WorkArea->Type == BuildArea
	// Destory the WorkArea
	
	 FHitResult HitResult;

	// Get the hit result under the cursor
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		// Check if the hit actor is of type AWorkArea
		AWorkArea* WorkArea = Cast<AWorkArea>(HitResult.GetActor());

		UE_LOG(LogTemp, Warning, TEXT("WorkArea->TeamId: %d"), WorkArea->TeamId);
		UE_LOG(LogTemp, Warning, TEXT("SelectableTeamId: %d"), SelectableTeamId);
		if (WorkArea && WorkArea->Type == WorkAreaData::BuildArea && WorkArea->TeamId == SelectableTeamId) // Assuming EWorkAreaType is an enum with 'BuildArea'
		{
			// Destroy the WorkArea
			//WorkArea->Destroy();
			DestoryWorkAreaOnServer(WorkArea);
		}
	}
	
}

void AExtendedControllerBase::LeftClickPressed()
{
	LeftClickIsPressed = true;

	if(AltIsPressed)
	{
		DestoryWorkArea();
	}else if (AttackToggled) {
		AttackToggled = false;
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

		int32 NumUnits = SelectedUnits.Num();
		int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)NumUnits));
		
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			int32 Row = i / GridSize;     // Row index
			int32 Col = i % GridSize;     // Column index

			FVector RunLocation = Hit.Location + FVector(Col * 100, Row * 100, 0.f);  // Adjust x and y positions equally for a square grid
		
			if(SetBuildingWaypoint(RunLocation, SelectedUnits[i]))
			{
				// Do Nothing
			}else
				LeftClickAttack_Implementation(SelectedUnits[i], RunLocation);
		}
		
	}
	else {
		DropWorkArea();
		LeftClickSelect();

		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);
		
		if (Hit_Pawn.bBlockingHit && HUDBase)
		{
			AActor* HitActor = Hit_Pawn.GetActor();
			
			if(!HitActor->IsA(ALandscape::StaticClass()))
				ClickedActor = Hit_Pawn.GetActor();
			else
				ClickedActor = nullptr;
			
			AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
			const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Hit_Pawn.GetActor());
			
			if (UnitBase && (UnitBase->TeamId == SelectableTeamId || SelectableTeamId == 0) && !SUnit)
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

void AExtendedControllerBase::LeftClickReleased()
{
	LeftClickIsPressed = false;
	HUDBase->bSelectFriendly = false;
	SelectedUnits = HUDBase->SelectedUnits;
	DropUnitBase();

	if(Cast<AExtendedCameraBase>(GetPawn())->TabToggled) SetWidgets(0);
}


void AExtendedControllerBase::DestroyDraggedArea_Implementation(AWorkingUnitBase* Worker)
{
	if(!Worker->CurrentDraggedWorkArea)
	{
		UE_LOG(LogTemp, Error, TEXT("DraggedArea is null! Cannot Destroy."));
		return;
	}

	Worker->CurrentDraggedWorkArea->PlannedBuilding = true;
	Worker->CurrentDraggedWorkArea->RemoveAreaFromGroup();
	Worker->CurrentDraggedWorkArea->Destroy();
	Worker->CurrentDraggedWorkArea = nullptr;
}

void AExtendedControllerBase::RightClickPressed()
{
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	
	
	if(!SelectedUnits.Num() || !SelectedUnits[0]->CurrentDraggedWorkArea)
		if(!CheckClickOnWorkArea(Hit))
		{
			RunUnitsAndSetWaypoints(Hit);
		}

	if (SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
	}
}

void AExtendedControllerBase::StopWork_Implementation(AWorkingUnitBase* Worker)
{
	if(Worker && Worker->GetUnitState() == UnitData::Build && Worker->BuildArea)
	{
		Worker->BuildArea->StartedBuilding = false;
		Worker->BuildArea->PlannedBuilding = false;
		AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

		if(ResourceGameMode)
		{
			ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, Worker->BuildArea->ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, Worker->BuildArea->ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, Worker->BuildArea->ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, Worker->BuildArea->ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, Worker->BuildArea->ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, Worker->BuildArea->ConstructionCost.LegendaryCost);
		}
	}
}

void AExtendedControllerBase::StopWorkOnSelectedUnit()
{
	if(SelectedUnits.Num() && SelectedUnits[0])
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
		StopWork(Worker);
	}
}
void AExtendedControllerBase::SendWorkerToResource_Implementation(AWorkingUnitBase* Worker, AWorkArea* WorkArea)
{
	if(Worker)
	{
		Worker->ResourcePlace = WorkArea;
		Worker->SetUnitState(UnitData::GoToResourceExtraction);
	}
}

void AExtendedControllerBase::SendWorkerToWorkArea_Implementation(AWorkingUnitBase* Worker, AWorkArea* WorkArea)
{
	Worker->BuildArea = WorkArea;
	// Check if the worker is overlapping with the build area
	if (Worker->IsOverlappingActor(Worker->BuildArea))
	{
		// If they are overlapping, set the state to 'Build'
		Worker->SetUnitState(UnitData::Build);
	}
	else
	{
		// If they are not overlapping, set the state to 'GoToBuild'
		Worker->SetUnitState(UnitData::GoToBuild);
	}
}
bool AExtendedControllerBase::CheckClickOnWorkArea(FHitResult Hit_Pawn)
{
	StopWorkOnSelectedUnit();
	
	if (Hit_Pawn.bBlockingHit && HUDBase)
	{
		AActor* HitActor = Hit_Pawn.GetActor();
		
		AWorkArea* WorkArea = Cast<AWorkArea>(HitActor);

		if(WorkArea)
		{
			TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkArea->Type;
		
			bool isResourceExtractionArea = Type == WorkAreaData::Primary || Type == WorkAreaData::Secondary || 
									 Type == WorkAreaData::Tertiary || Type == WorkAreaData::Rare ||
									 Type == WorkAreaData::Epic || Type == WorkAreaData::Legendary;


			if(WorkArea && isResourceExtractionArea)
			{
				for (int32 i = 0; i < SelectedUnits.Num(); i++)
				{
					if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead)
					{
						
						AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[i]);

						SendWorkerToResource(Worker, WorkArea);
					}
				}
			} else if(WorkArea && WorkArea->Type == WorkAreaData::BuildArea)
			{
				if(SelectedUnits.Num() && SelectedUnits[0])
				{
					AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
					if(Worker && (Worker->TeamId == WorkArea->TeamId || WorkArea->TeamId == 0))
					{
						SendWorkerToWorkArea(Worker, WorkArea);
					}
				}
			}

			if(WorkArea) return true;
		}
		
	}

	return false;
}
