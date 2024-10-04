// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/ExtendedControllerBase.h"

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


void AExtendedControllerBase::SpawnWorkArea_Implementation(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint)
{
    if (WorkAreaClass)
    {
        FVector MousePosition, MouseDirection;
        DeprojectMousePositionToWorld(MousePosition, MouseDirection);

        // Raycast from the mouse position into the scene to find the ground
        FVector Start = MousePosition;
        FVector End = Start + MouseDirection * 5000.f; // Extend to a maximum reasonable distance

        FHitResult HitResult;
        FCollisionQueryParams CollisionParams;
        CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing

        // Perform the raycast
        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

 
        FRotator SpawnRotation = FRotator::ZeroRotator;
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = GetPawn();
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    	
        AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, SpawnLocation, SpawnRotation, SpawnParams);
        if (SpawnedWorkArea)
        {
        	if(Waypoint) SpawnedWorkArea->NextWaypoint = Waypoint;
        	SpawnedWorkArea->TeamId = SelectableTeamId;
            CurrentDraggedWorkArea = SpawnedWorkArea;
        }
    }
}

void AExtendedControllerBase::MoveWorkArea_Implementation(float DeltaSeconds)
{
	// Check if there's a CurrentDraggedWorkArea
	if (CurrentDraggedWorkArea)
	{
		FVector MousePosition, MouseDirection;
		DeprojectMousePositionToWorld(MousePosition, MouseDirection);

		// Raycast from the mouse position into the scene to find the ground
		FVector Start = MousePosition;
		FVector End = Start + MouseDirection * 5000.f; // Extend to a maximum reasonable distance

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing
		CollisionParams.AddIgnoredActor(CurrentDraggedWorkArea); // Ignore the dragged actor in the raycast

		// Perform the raycast
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

		// Check if something was hit
		if (bHit && HitResult.GetActor() != nullptr)
		{
			CurrentDraggedGround = HitResult.GetActor();
			// Update the work area's position to the hit location
			FVector NewActorPosition = HitResult.Location;
			NewActorPosition.Z += 50.f; // Adjust the Z offset if needed
			CurrentDraggedWorkArea->SetActorLocation(NewActorPosition);
		}
	}
}

void AExtendedControllerBase::DropWorkArea()
{
	if (CurrentDraggedWorkArea && CurrentDraggedWorkArea->PlannedBuilding == false)
	{
		// Get all actors overlapping with the CurrentDraggedWorkArea
		TArray<AActor*> OverlappingActors;
		CurrentDraggedWorkArea->GetOverlappingActors(OverlappingActors);

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
			CurrentDraggedWorkArea->Destroy();
		}
		else
		{
			if(SelectedUnits.Num() && SelectedUnits[0])
			{
				AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
				if(Worker)
				{
					Worker->BuildArea = CurrentDraggedWorkArea;
					Worker->BuildArea->TeamId = SelectedUnits[0]->TeamId;
					Worker->BuildArea->PlannedBuilding = true;
					Worker->BuildArea->AddAreaToGroup();
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
			}
		}

		CurrentDraggedWorkArea = nullptr;
	}
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
		if (WorkArea && WorkArea->Type == WorkAreaData::BuildArea && WorkArea->TeamId == SelectableTeamId) // Assuming EWorkAreaType is an enum with 'BuildArea'
		{
			// Destroy the WorkArea
			WorkArea->Destroy();
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

void AExtendedControllerBase::RightClickPressed()
{
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	

	if(!CurrentDraggedWorkArea)
		if(!CheckClickOnWorkArea(Hit))
		{
			RunUnitsAndSetWaypoints(Hit);
		}

	if (CurrentDraggedWorkArea)
	{
		CurrentDraggedWorkArea->PlannedBuilding = true;
		CurrentDraggedWorkArea->RemoveAreaFromGroup();
		CurrentDraggedWorkArea->Destroy();
	}
}

void AExtendedControllerBase::StopWork()
{
	if(SelectedUnits.Num() && SelectedUnits[0])
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
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
}

bool AExtendedControllerBase::CheckClickOnWorkArea(FHitResult Hit_Pawn)
{
	StopWork();
	
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

						if(Worker)
						{
							Worker->ResourcePlace = WorkArea;
							Worker->SetUnitState(UnitData::GoToResourceExtraction);
						}
					}
				}
			} else if(WorkArea && WorkArea->Type == WorkAreaData::BuildArea)
			{
				if(SelectedUnits.Num() && SelectedUnits[0])
				{
					AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
					if(Worker && (Worker->TeamId == WorkArea->TeamId || WorkArea->TeamId == 0))
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
				}
			}

			if(WorkArea) return true;
		}
		
	}

	return false;
}
