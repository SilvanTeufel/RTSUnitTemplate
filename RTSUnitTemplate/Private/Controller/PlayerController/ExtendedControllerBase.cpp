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

void AExtendedControllerBase::ActivateAbilitiesByIndex_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (!UnitBase)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("HitResult: %s"), *HitResult.Location.ToString());
	switch (AbilityArrayIndex)
	{
	case 0:
		ActivateDefaultAbilities(UnitBase, InputID, HitResult);
		break;
	case 1:
		ActivateSecondAbilities(UnitBase, InputID, HitResult);
		break;
	case 2:
		ActivateThirdAbilities(UnitBase, InputID, HitResult);
		break;
	case 3:
		ActivateFourthAbilities(UnitBase, InputID, HitResult);
		break;
	default:
		// Optionally handle invalid indices
			break;
	}
}

void AExtendedControllerBase::ActivateDefaultAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if(UnitBase && UnitBase->DefaultAbilities.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("HitResult: %s"), *HitResult.Location.ToString());
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->DefaultAbilities, HitResult);
	}
	
}

void AExtendedControllerBase::ActivateSecondAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->SecondAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->SecondAbilities, HitResult);
	}
}

void AExtendedControllerBase::ActivateThirdAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->ThirdAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->ThirdAbilities, HitResult);
	}
}

void AExtendedControllerBase::ActivateFourthAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->FourthAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->FourthAbilities, HitResult);
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
			ActivateDefaultAbilities_Implementation(Unit, InputID, FHitResult());
		}
	}
}


TArray<TSubclassOf<UGameplayAbilityBase>> AExtendedControllerBase::GetAbilityArrayByIndex()
{
	if (!SelectedUnits.Num() || SelectedUnits[0]) return SelectedUnits[0]->DefaultAbilities;
	
	switch (AbilityArrayIndex)
	{
	case 0:
		return SelectedUnits[0]->DefaultAbilities;
		break;
	case 1:
		return SelectedUnits[0]->SecondAbilities;
		break;
	case 2:
		return SelectedUnits[0]->ThirdAbilities;
		break;
	case 3:
		return SelectedUnits[0]->FourthAbilities;
		break;
	default:
		return SelectedUnits[0]->DefaultAbilities;
			break;
	}
}

void AExtendedControllerBase::AddAbilityIndex(int Add)
{
	
if (!SelectedUnits.Num() || !SelectedUnits[0]) return;
	
	int NewIndex = AbilityArrayIndex+=Add;


	if (NewIndex < 0)
		NewIndex = MaxAbilityArrayIndex;

	if (NewIndex == 0 && !SelectedUnits[0]->DefaultAbilities.Num())
		NewIndex+=Add;
	
	if (NewIndex == 1 && !SelectedUnits[0]->SecondAbilities.Num())
		NewIndex+=Add;
	 
	if (NewIndex == 2 && !SelectedUnits[0]->ThirdAbilities.Num())
		NewIndex+=Add;
	
	if (NewIndex == 3 && !SelectedUnits[0]->FourthAbilities.Num())
		NewIndex+=Add;

	if (NewIndex == 2 && !SelectedUnits[0]->ThirdAbilities.Num())
		NewIndex+=Add;

	if (NewIndex == 1 && !SelectedUnits[0]->SecondAbilities.Num())
		NewIndex+=Add;

	if (NewIndex == 0 && !SelectedUnits[0]->DefaultAbilities.Num())
		NewIndex+=Add;

	if (NewIndex > MaxAbilityArrayIndex)
		NewIndex = 0;


	AbilityArrayIndex=NewIndex;

	UE_LOG(LogTemp, Warning, TEXT("MaxAbilityArrayIndex: %d"), MaxAbilityArrayIndex);
	UE_LOG(LogTemp, Warning, TEXT("Final AbilityArrayIndex after wrap: %d"), AbilityArrayIndex);
}

void AExtendedControllerBase::ApplyMovementInputToUnit_Implementation(const FVector& Direction, float Scale, AUnitBase* Unit, int TeamId)
{
	UE_LOG(LogTemp, Warning, TEXT("ApplyMovementInputToUnit_Implementation: %d"), AbilityArrayIndex);

	if (Unit && (Unit->TeamId == TeamId))
	{
		if (Unit->GetController()) // Ensure the unit has a valid Controller
		{
			UE_LOG(LogTemp, Warning, TEXT("Direction: %s"), *Direction.ToString());
			UE_LOG(LogTemp, Warning, TEXT("Scale: %f"), Scale);
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
			HUDBase->SetUnitSelected(ClosestUnit);
			ActivateDefaultAbilities_Implementation(ClosestUnit, InputID, FHitResult());
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
	ActivateDefaultAbilities_Implementation(ClosestUnit, InputID, FHitResult());
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
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	
	if (SelectedUnits.Num() > 0)
	{
		for (AGASUnit* SelectedUnit : SelectedUnits)
		{
			if (SelectedUnit)
			{
				//ActivateDefaultAbilities(SelectedUnit, InputID);
				ActivateAbilitiesByIndex_Implementation(SelectedUnit, InputID, Hit);
			}
		}
	}else if (CameraUnitWithTag)
	{
		CameraUnitWithTag->SetSelected();
		SelectedUnits.Emplace(CameraUnitWithTag);
		HUDBase->SetUnitSelected(CameraUnitWithTag);
		ActivateAbilitiesByIndex_Implementation(CameraUnitWithTag, InputID, Hit);
	}
	else if(HUDBase)
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

void AExtendedControllerBase::SnapToActor(AActor* DraggedActor, AActor* OtherActor)
{
    if (!DraggedActor || !OtherActor || DraggedActor == OtherActor)
        return;

    // 1) Get the dragged mesh
    UStaticMeshComponent* DraggedMesh = DraggedActor->FindComponentByClass<UStaticMeshComponent>();
    if (!DraggedMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("DraggedActor has no StaticMeshComponent!"));
        return;
    }

    // 2) Get the other mesh
    UStaticMeshComponent* OtherMesh = OtherActor->FindComponentByClass<UStaticMeshComponent>();
    if (!OtherMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("OtherActor has no StaticMeshComponent!"));
        return;
    }

    // 3) Compute each mesh’s bounds in world space
    //    (You could also use DraggedMesh->Bounds directly, but CalcBounds is explicit.)
    FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
    FBoxSphereBounds OtherBounds   = OtherMesh->CalcBounds(OtherMesh->GetComponentTransform());

    // 4) Extract the centers and half-extents
    FVector DraggedCenter = DraggedBounds.Origin;
    FVector DraggedExtent = DraggedBounds.BoxExtent; // half-size in X, Y, Z
    FVector OtherCenter   = OtherBounds.Origin;
    FVector OtherExtent   = OtherBounds.BoxExtent;

    // 5) Decide on a small gap so they don’t overlap visually
    float Gap = 10.f;

    // 6) Compute how much to offset on X and Y so they "just touch"
    //    (i.e. sum of half-extents along that axis + Gap)
    float XOffset = DraggedExtent.X + OtherExtent.X + Gap;
    float YOffset = DraggedExtent.Y + OtherExtent.Y + Gap;

    // 7) Determine which axis (X or Y) is already closer
    //    - dx < dy => line them up on X; offset on Y
    //    - else => line them up on Y; offset on X
    float dx = FMath::Abs(DraggedCenter.X - OtherCenter.X);
    float dy = FMath::Abs(DraggedCenter.Y - OtherCenter.Y);

    // We’ll start with the current ActorLocation’s Z so we only adjust X/Y in snapping.
    FVector SnappedPos = DraggedActor->GetActorLocation();

    if (dx < dy)
    {
        // They’re closer on X, so snap them to the same X
        SnappedPos.X = OtherCenter.X;

        // Offset Y so they’re side-by-side
        float SignY = (DraggedCenter.Y >= OtherCenter.Y) ? 1.f : -1.f;
        SnappedPos.Y = OtherCenter.Y + SignY * YOffset;
    }
    else
    {
        // They’re closer on Y, so snap them to the same Y
        SnappedPos.Y = OtherCenter.Y;

        // Offset X so they’re side-by-side
        float SignX = (DraggedCenter.X >= OtherCenter.X) ? 1.f : -1.f;
        SnappedPos.X = OtherCenter.X + SignX * XOffset;
    }

    // 8) (Optional) If you need a specific Z (e.g., ground level), set it here.
    //    Otherwise, we keep the dragged actor’s original Z.

    // 9) Finally, move the dragged actor
    DraggedActor->SetActorLocation(SnappedPos);
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

			float Distance = FVector::Dist(
				HitResult.Location,
				SelectedUnits[0]->CurrentDraggedWorkArea->GetActorLocation()
			);

			if (Distance < 100.f)
			if (UStaticMeshComponent* Mesh = SelectedUnits[0]->CurrentDraggedWorkArea->FindComponentByClass<UStaticMeshComponent>())
			{
				// 1) Get bounding sphere
				FBoxSphereBounds MeshBounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
				float OverlapRadius = MeshBounds.SphereRadius;

				// 2) Run the sphere overlap
				TArray<AActor*> OverlappedActors;
				bool bAnyOverlap = UKismetSystemLibrary::SphereOverlapActors(
					this,                                       
					SelectedUnits[0]->CurrentDraggedWorkArea->GetActorLocation(), 
					OverlapRadius,
					TArray<TEnumAsByte<EObjectTypeQuery>>(),
					AActor::StaticClass(),
					TArray<AActor*>(),
					OverlappedActors
				);

				// 3) Check the overlapped actors
				if (bAnyOverlap && OverlappedActors.Num() > 0)
				{
					for (AActor* OverlappedActor : OverlappedActors)
					{
						if (!OverlappedActor || OverlappedActor == SelectedUnits[0]->CurrentDraggedWorkArea)
							continue;

						AWorkArea* OverlappedWorkArea = Cast<AWorkArea>(OverlappedActor);
						ABuildingBase* OverlappedBuilding = Cast<ABuildingBase>(OverlappedActor);
						if (OverlappedWorkArea || OverlappedBuilding)
						{
							// Snap logic
							SnapToActor(SelectedUnits[0]->CurrentDraggedWorkArea, OverlappedActor);
							// break if you only want one
							return;
						}
					}
				}
			}
			
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
