// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/ExtendedControllerBase.h"

#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"

void AExtendedControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	MoveDraggedUnit_Implementation(DeltaSeconds);
	MoveWorkArea_Implementation(DeltaSeconds);
}


void AExtendedControllerBase::SpawnWorkArea_Implementation(TSubclassOf<AWorkArea> WorkAreaClass)
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

        FVector SpawnWorkAreaLocation;
        if (bHit)
        {
            SpawnWorkAreaLocation = HitResult.Location;
        }
        else
        {
            // If nothing was hit, spawn at a default location in front of the player
            APawn* ControlledPawn = GetPawn();
            if (ControlledPawn)
            {
                SpawnWorkAreaLocation = ControlledPawn->GetActorLocation() + ControlledPawn->GetActorForwardVector() * 200.f;
            }
            else
            {
                SpawnWorkAreaLocation = FVector::ZeroVector;
            }
        }

        FRotator SpawnRotation = FRotator::ZeroRotator;
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = GetPawn();
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, SpawnLocation, SpawnRotation, SpawnParams);
        if (SpawnedWorkArea)
        {
            CurrentDraggedWorkArea = SpawnedWorkArea;

            // Optionally, you might want to set some initial properties or states on the spawned WorkArea here

            // Start moving the WorkArea under the mouse cursor
            // You can call MoveWorkArea function or set up a flag to start moving it in Tick
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
			// Update the work area's position to the hit location
			FVector NewActorPosition = HitResult.Location;
			NewActorPosition.Z += 50.f; // Adjust the Z offset if needed
			CurrentDraggedWorkArea->SetActorLocation(NewActorPosition);
		}
	}
}

void AExtendedControllerBase::DropWorkArea()
{
	if (CurrentDraggedWorkArea)
	{


		if(SelectedUnits[0])
		{
			AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
			if(Worker)
			{
				Worker->BuildArea = CurrentDraggedWorkArea;
				Worker->SetUnitState(UnitData::GoToBuild);
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

void AExtendedControllerBase::LeftClickPressed()
{
	LeftClickIsPressed = true;
	if (AttackToggled) {
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