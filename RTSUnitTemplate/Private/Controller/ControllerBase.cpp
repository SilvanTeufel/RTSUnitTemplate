// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/ControllerBase.h"
#include "NavigationSystem.h" // Include this for navigation functions
#include "Controller/CameraControllerBase.h"
#include "Core/UnitData.h"
#include "AIController.h"
#include "Landscape.h"
#include "Actors/EffectArea.h"
#include "Actors/MissileRain.h"
#include "Actors/UnitSpawnPlatform.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "NavMesh/NavMeshPath.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameModes/ResourceGameMode.h"
#include "Engine/Engine.h"

AControllerBase::AControllerBase() {
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}


void AControllerBase::BeginPlay() {

		ToggleFPSDisplay(ShowFPS);
		CameraBase = Cast<ACameraBase>(GetPawn());
		HUDBase = Cast<APathProviderHUD>(GetHUD());
		if(HUDBase && HUDBase->StopLoading) CameraBase->DeSpawnLoadingWidget();
}

void AControllerBase::ToggleFPSDisplay(bool bEnable)
{
	if (bEnable)
	{
		// Start the timer to call DisplayFPS every 2 seconds
		GetWorldTimerManager().SetTimer(FPSTimerHandle, this, &AControllerBase::DisplayFPS, 2.0f, true);
	}
	else
	{
		// Stop the timer if it's running
		GetWorldTimerManager().ClearTimer(FPSTimerHandle);
	}
}

void AControllerBase::DisplayFPS()
{
	float DeltaTime = GetWorld()->DeltaTimeSeconds;
	float FPS = 1.0f / DeltaTime;
	FString FPSMessage = FString::Printf(TEXT("FPS: %f"), FPS);

	// Retrieve the count of all units from HUDBase or a similar class
	int UnitsCount = HUDBase->AllUnits.Num();
	FString UnitsMessage = FString::Printf(TEXT("Unit Count: %d"), UnitsCount);

	// Combine FPS and Unit Count messages
	FString CombinedMessage = FPSMessage + TEXT(" | ") + UnitsMessage;

	// Display the combined message on screen
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, CombinedMessage, true);
	}
}

void AControllerBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AControllerBase, CameraBase);
	DOREPLIFETIME(AControllerBase, UseUnrealEnginePathFinding);
	DOREPLIFETIME(AControllerBase, MissileRainClass);
	DOREPLIFETIME(AControllerBase, EffectAreaClass);
	DOREPLIFETIME(AControllerBase, IsShiftPressed);
	DOREPLIFETIME(AControllerBase, AttackToggled);
	DOREPLIFETIME(AControllerBase, IsStrgPressed);
	DOREPLIFETIME(AControllerBase, IsSpacePressed);
	DOREPLIFETIME(AControllerBase, AltIsPressed);
	DOREPLIFETIME(AControllerBase, LeftClickIsPressed);
	DOREPLIFETIME(AControllerBase, LockCameraToUnit);
	DOREPLIFETIME(AControllerBase, AIsPressedState);
	DOREPLIFETIME(AControllerBase, DIsPressedState);
	DOREPLIFETIME(AControllerBase, WIsPressedState);
	DOREPLIFETIME(AControllerBase, SIsPressedState);
	DOREPLIFETIME(AControllerBase, MiddleMouseIsPressed);
	DOREPLIFETIME(AControllerBase, SelectableTeamId);
}

void AControllerBase::SetupInputComponent() {

	Super::SetupInputComponent();

}


void AControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if(HUDBase)
	if((HUDBase->CreatedGridAndDijkstra || HUDBase->StopLoading) && CameraBase)
	{
		CameraBase->BlockControls = false;
		CameraBase->DeSpawnLoadingWidget();
	}

	if(HUDBase) SelectedUnitCount = HUDBase->SelectedUnits.Num();
	
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	
	//if(IsShiftPressed)
		//ClickLocation = Hit.Location;
	
	if(AttackToggled)
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if(SelectedUnits[i] && SelectedUnits[i]->GetUnitState() == UnitData::Idle && SelectedUnits[i]->ToggleUnitDetection)
		HUDBase->ControllDirectionToMouse(SelectedUnits[i], Hit);
	}

	TArray<FPathPoint> PathPoints;

	if(HUDBase && !HUDBase->DisablePathFindingOnEnemy)
	for (int32 i = 0; i < HUDBase->EnemyUnitBases.Num(); i++)
		if( HUDBase->EnemyUnitBases[i] && !HUDBase->EnemyUnitBases[i]->TeamId &&  HUDBase->EnemyUnitBases[i]->DijkstraSetPath)
		{
			SetRunLocationUseDijkstraForAI(HUDBase->EnemyUnitBases[i]->DijkstraEndPoint, HUDBase->EnemyUnitBases[i]->DijkstraStartPoint, HUDBase->EnemyUnitBases, PathPoints, i);
			HUDBase->EnemyUnitBases[i]->DijkstraSetPath = false;
		}

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
		/*
		FVector MousePosition, MouseDirection;
		DeprojectMousePositionToWorld(MousePosition, MouseDirection);

		FVector NewActorPosition = MousePosition + MouseDirection*300; // 300 units in front of the camera
		CurrentDraggedUnitBase->SetActorLocation(NewActorPosition);
		*/
	}
}

void AControllerBase::ShiftPressed()
{
	IsShiftPressed = true;
}

void AControllerBase::ShiftReleased()
{
	IsShiftPressed = false;
}

void AControllerBase::SelectUnit(int Index)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if(SelectedUnits[i] && i != Index) SelectedUnits[i]->SetDeselected();
	}
	SelectedUnits.Empty();
	SelectedUnits.Add(HUDBase->SelectedUnits[Index]);
	HUDBase->SelectedUnits[Index]->SetSelected();

}

void AControllerBase::LeftClickAMoveUEPF_Implementation(AUnitBase* Unit, FVector Location)
{
	DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Green, false, 1.5, 0, 1);
	SetUnitState_Replication(Unit,1);
	MoveToLocationUEPathFinding(Unit, Location);
}

void AControllerBase::LeftClickAMove_Implementation(AUnitBase* Unit, FVector Location)
{
	DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Green, false, 1.5, 0, 1);
	SetUnitState_Replication(Unit,1);
	Unit->RunLocationArray.Empty();
	Unit->RunLocationArrayIterator = 0;
	SetRunLocation(Unit, Location);
}


void AControllerBase::LeftClickAttack_Implementation(AUnitBase* Unit, FVector Location)
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
					
				if (Unit &&Unit->UnitState != UnitData::Dead)
				{
					LeftClickAMoveUEPF(Unit, Location);
				}
					
			}else
			{
				LeftClickAMove(Unit, Location);
			}
		}else if(UseUnrealEnginePathFinding)
		{
			if (Unit &&Unit->UnitState != UnitData::Dead)
			{
				/// A-Move Units ///
				LeftClickAMoveUEPF(Unit, Location);
			}
					
		}
			
	}
}

void AControllerBase::LeftClickSelect_Implementation()
{
	FHitResult Hit_IPoint;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit_IPoint);
	
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if(SelectedUnits[i])
			SelectedUnits[i]->SetDeselected();
	}
		
	SelectedUnits.Empty();

}

void AControllerBase::LeftClickPressed()
{
	LeftClickIsPressed = true;
	if (AttackToggled) {
		AttackToggled = false;
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
			LeftClickAttack_Implementation(SelectedUnits[i], RunLocation);
		}
		
	}
	else {
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

void AControllerBase::LeftClickReleased()
{
	LeftClickIsPressed = false;
	HUDBase->bSelectFriendly = false;
	SelectedUnits = HUDBase->SelectedUnits;
	DropUnitBase();
	SetWidgets(0);
}

void AControllerBase::DragUnitBase(AUnitBase* UnitToDrag)
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

void AControllerBase::DropUnitBase()
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

void AControllerBase::SetWidgets(int Index)
{
	SelectedUnits = HUDBase->SelectedUnits;
	
	if (SelectedUnits.Num()) {
	
		AUnitBase* UnitBase = Cast<AUnitBase>(SelectedUnits[Index]);
		AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
		
		if(UnitBase && ExtendedCameraBase)
		{
			ExtendedCameraBase->SetUserWidget(UnitBase);
			ExtendedCameraBase->SetSelectorWidget(Index, UnitBase);
		}else
		{
			ExtendedCameraBase->SetUserWidget(nullptr);
		}
	}
}

void AControllerBase::SetRunLocation_Implementation(AUnitBase* Unit, const FVector& DestinationLocation)
{
	Unit->SetRunLocation(DestinationLocation);
}

void AControllerBase::MoveToLocationUEPathFinding_Implementation(AUnitBase* Unit, const FVector& DestinationLocation)
{

	if(!HasAuthority())
	{
		return;
	}
	
	if (!Unit || !Unit->GetCharacterMovement())
	{
		return;
	}

	// Check if we have a valid AI controller for the unit
	AAIController* AIController = Cast<AAIController>(Unit->GetController());
	if (!AIController)
	{
		return;
	}

	// Check if we have a valid navigation system
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return;
	}
		
	SetRunLocation(Unit, DestinationLocation);
	Unit->UEPathfindingUsed = true;
	// Move the unit to the destination location using the navigation system
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(DestinationLocation);
	MoveRequest.SetAcceptanceRadius(5.0f); // Set an acceptance radius for reaching the destination
	
	FNavPathSharedPtr NavPath;
	
	AIController->MoveTo(MoveRequest, &NavPath);
	
	if(NavPath)
	{
		FNavMeshPath* NavMeshPath = NavPath->CastPath<FNavMeshPath>();
		if (NavMeshPath)
		{
			NavMeshPath->OffsetFromCorners(UEPathfindingCornerOffset);
		}
	}
}

void AControllerBase::SetUnitState_Replication_Implementation(AUnitBase* Unit, int State)
{
	if(Unit)
	switch (State)
	{
	case 0:
		Unit->SetUnitState(UnitData::Idle);
		break;

	case 1:
		Unit->SetUnitState(UnitData::Run);
		break;

	case 2:
		Unit->SetUnitState(UnitData::Attack);
		break;
		
	case 3:
		Unit->SetUnitState(UnitData::Chase);
		break;
		// ... other cases

		default:
			// Handle invalid state
			break;
	}
}

void AControllerBase::SetToggleUnitDetection_Implementation(AUnitBase* Unit, bool State)
{
	Unit->SetToggleUnitDetection(State);
}
void AControllerBase::RightClickRunShift_Implementation(AUnitBase* Unit, FVector Location)
{
	DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Green, false, 1.5, 0, 1);
	if(!Unit->RunLocationArray.Num())
	{
		SetRunLocation(Unit, Location);
		Unit->UEPathfindingUsed = false;

		SetUnitState_Replication(Unit,1);
	}
						
	Unit->RunLocationArray.Add(Location);
	Unit->UnitsToChase.Empty();
	Unit->UnitToChase = nullptr;
	Unit->SetToggleUnitDetection(false);
}

void AControllerBase::RightClickRunUEPF_Implementation(AUnitBase* Unit, FVector Location)
{
	DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Green, false, 1.5, 0, 1);
	MoveToLocationUEPathFinding(Unit, Location);
	SetUnitState_Replication(Unit,1);
	SetToggleUnitDetection(Unit, false);
}

void AControllerBase::RightClickRunDijkstraPF_Implementation(AUnitBase* Unit, FVector Location, int Counter)
{
	TArray<FPathPoint> PathPoints;

	FVector UnitLocation = Unit->GetActorLocation();

	DrawDebugCircle(GetWorld(), FVector(Location.X, Location.Y, Location.Z+2.f), 15, 5, FColor::Green, false, 1.5, 0, 1, FVector(0, 1, 0), FVector(1, 0, 0));
	if(Unit->GetUnitState() != UnitData::Run)
		Unit->SetWalkSpeed(0.f);

	Unit->UEPathfindingUsed = false;
	SetUnitState_Replication(Unit,1);
	Unit->RunLocationArray.Empty();
	Unit->RunLocationArrayIterator = 0;
	SetRunLocation(Unit, Location);
	Unit->SetToggleUnitDetection(false);

	float Range = FVector::Dist(UnitLocation, Location);
 
	if(!HUDBase->DisablePathFindingOnFriendly && Range >= HUDBase->RangeThreshold && !HUDBase->IsLocationInNoPathFindingAreas(Location))
		SetRunLocationUseDijkstra(Location, UnitLocation, SelectedUnits, PathPoints, Counter);
}

void AControllerBase::RightClickPressed()
{
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	
	if(!CheckResourceExtraction(Hit)) RunUnits(Hit);
}

bool AControllerBase::CheckResourceExtraction(FHitResult Hit_Pawn)
{
	//UE_LOG(LogTemp, Log, TEXT("CheckResourceExtraction called"));
	if (Hit_Pawn.bBlockingHit && HUDBase)
	{
		//UE_LOG(LogTemp, Log, TEXT("Hit_Pawn is blocking and HUDBase is valid"));
		AActor* HitActor = Hit_Pawn.GetActor();

		if(HitActor)  UE_LOG(LogTemp, Log, TEXT("HitActor Name: %s, Type: %s"), *HitActor->GetName(), *HitActor->GetClass()->GetName());

		
		AWorkArea* WorkArea = Cast<AWorkArea>(HitActor);

		if(WorkArea)
		{
			//UE_LOG(LogTemp, Log, TEXT("HitActor is a WorkArea"));
			TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkArea->Type;
		
			bool isResourceExtractionArea = Type == WorkAreaData::Primary || Type == WorkAreaData::Secondary || 
									 Type == WorkAreaData::Tertiary || Type == WorkAreaData::Rare ||
									 Type == WorkAreaData::Epic || Type == WorkAreaData::Legendary;

			//UE_LOG(LogTemp, Log, TEXT("WorkArea type: %d, isResourceExtractionArea: %s"), static_cast<int32>(Type), isResourceExtractionArea ? TEXT("true") : TEXT("false"));
			
			if(WorkArea && isResourceExtractionArea)
			{
				for (int32 i = 0; i < SelectedUnits.Num(); i++)
				{
					if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead)
					{
						//UE_LOG(LogTemp, Log, TEXT("Selected unit %d is valid and not dead"), i);
						
						AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(SelectedUnits[i]);

						if(WorkingUnit)
						{
							//UE_LOG(LogTemp, Log, TEXT("Unit %d assigned to WorkArea and state set to GoToResourceExtraction"), i);
							WorkingUnit->ResourcePlace = WorkArea;
							WorkingUnit->SetUnitState(UnitData::GoToResourceExtraction);
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}


void AControllerBase::RunUnits(FHitResult Hit)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead) {
			FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
			if (IsShiftPressed) {
				RightClickRunShift_Implementation(SelectedUnits[i], RunLocation);
			}else if(UseUnrealEnginePathFinding && !SelectedUnits[i]->IsFlying)
			{
				RightClickRunUEPF_Implementation(SelectedUnits[i], RunLocation);
			}
			else {
				RightClickRunDijkstraPF_Implementation(SelectedUnits[i], RunLocation, i);
			}
		}
	}
}

void AControllerBase::SetRunLocationUseDijkstra(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i)
{
	HUDBase->SetNextDijkstraTo(Units, HitLocation);
	FHitResult LineHit;
	FVector LineEndLocation = FVector(HitLocation.X, HitLocation.Y, HitLocation.Z+10.f);
	GetWorld()->LineTraceSingleByChannel(LineHit, UnitLocation, LineEndLocation, HUDBase->TraceChannelProperty, HUDBase->QueryParams);
	
	float ZDistance_B = abs(Units[i]->Next_DijkstraPMatrixes.CenterPoint.Z - HitLocation.Z);
		
	if(LineHit.bBlockingHit && Units[i]->Next_DijkstraPMatrixes.Matrix.Num())
	{
		if(i == 0 && HUDBase->UseDijkstraOnlyOnFirstUnit)
		{
			if(ZDistance_B > 150.f) HUDBase->SetDijkstraWithClosestZDistance(Units[i], HitLocation.Z);
			PathPoints = HUDBase->GetPathReUseDijkstra( Units[i]->Next_DijkstraPMatrixes.Matrix, HitLocation, UnitLocation);
		}

		if(!HUDBase->UseDijkstraOnlyOnFirstUnit)
		{
			if(ZDistance_B > 150.f) HUDBase->SetDijkstraWithClosestZDistance(Units[i], HitLocation.Z);
			PathPoints = HUDBase->GetPathReUseDijkstra( Units[i]->Next_DijkstraPMatrixes.Matrix, HitLocation, UnitLocation);
		}
					
		if(PathPoints.Num())
		{
			SetRunLocation(Units[i], PathPoints[0].Point + FVector(i / 2 * 50, i % 2 * 50, 0.f));		
			for(int k = 1; k < PathPoints.Num(); k++)
			{
				Units[i]->RunLocationArray.Add(PathPoints[k].Point);
			}
						
			Units[i]->RunLocationArray.Add(HitLocation + FVector(i / 2 * 50, i % 2 * 50, 0.f));
		}
	}
}

void AControllerBase::SetRunLocationUseDijkstraForAI(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i)
{
	FHitResult LineHit;
	FVector LineEndLocation = FVector(HitLocation.X, HitLocation.Y, HitLocation.Z+10.f);
	GetWorld()->LineTraceSingleByChannel(LineHit, UnitLocation, LineEndLocation, HUDBase->TraceChannelProperty, HUDBase->QueryParams);
		
	if(LineHit.bBlockingHit && Units[i]->Next_DijkstraPMatrixes.Matrix.Num())
	{
		
		PathPoints = HUDBase->GetPathReUseDijkstra( Units[i]->Next_DijkstraPMatrixes.Matrix, HitLocation, UnitLocation);
					
		if(PathPoints.Num())
		{
			SetRunLocation(Units[i], PathPoints[0].Point);			
			for(int k = 1; k < PathPoints.Num(); k++)
			{
				Units[i]->RunLocationArray.Add(PathPoints[k].Point);
			}
						
			Units[i]->RunLocationArray.Add(HitLocation);
			Units[i]->FollowPath = true;
		}
	}else
	{
		Units[i]->FollowPath = false;
	}
}


void AControllerBase::SpacePressed()
{
	IsSpacePressed = true;
}

void AControllerBase::SpaceReleased()
{
	IsSpacePressed = false;
}

void AControllerBase::ToggleUnitDetection_Implementation(AUnitBase* Unit)
{
	if (Unit && Unit->UnitState != UnitData::Dead)
	{
		if(Unit)
			Unit->SetToggleUnitDetection(true);
	}
}


void AControllerBase::TPressed()
{
	if(!AttackToggled)
	{
		AttackToggled = true;
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			ToggleUnitDetection(SelectedUnits[i]);
		}
	}
}

void AControllerBase::AReleased()
{

}

void AControllerBase::JumpCamera()
{
	if (CameraBase && (IsStrgPressed || IsSpacePressed))
	{
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		CameraBase->JumpCamera(Hit);

	}
}


void AControllerBase::StrgPressed() {
	IsStrgPressed = true;
}

void AControllerBase::StrgReleased() {
	IsStrgPressed = false;
}

void AControllerBase::ZoomIn()
{
	if (CameraBase && IsStrgPressed)
	{
		CameraBase->ZoomIn(1.f);
	}
}

void AControllerBase::ZoomOut()
{
	if (CameraBase && IsStrgPressed)
	{
		CameraBase->ZoomOut(1.f);
	}
}


void AControllerBase::SpawnMissileRain(int TeamId, FVector Location) // FVector TargetLocation
{

		FTransform Transform;
		Transform.SetLocation(Location);
		Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator

		
		const auto MyMissileRain = Cast<AMissileRain>
							(UGameplayStatics::BeginDeferredActorSpawnFromClass
							(this, MissileRainClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

	if (MyMissileRain != nullptr)
	{
			MyMissileRain->TeamId = TeamId;

			UGameplayStatics::FinishSpawningActor(MyMissileRain, Transform);
	}
	
}

void AControllerBase::SpawnEffectArea(int TeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, AUnitBase* ActorToLockOn)
{

	FTransform Transform;
	Transform.SetLocation(Location);
	Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator

		
	const auto MyEffectArea = Cast<AEffectArea>
						(UGameplayStatics::BeginDeferredActorSpawnFromClass
						(this, EAClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	
	if (MyEffectArea != nullptr)
	{
		MyEffectArea->TeamId = TeamId;
		MyEffectArea->Mesh->OnComponentBeginOverlap.AddDynamic(MyEffectArea, &AEffectArea::OnOverlapBegin);

		// Apply scale to the Mesh
		MyEffectArea->Mesh->SetWorldScale3D(Scale);

		if(ActorToLockOn)
		{
			MyEffectArea->AttachToComponent(ActorToLockOn->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("rootSocket"));
		}
		
		UGameplayStatics::FinishSpawningActor(MyEffectArea, Transform);
	}
	
}

float AControllerBase::GetResource(int TeamId, EResourceType RType)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode) return 0;
	
	return GameMode->GetResource(TeamId, RType);
}

void AControllerBase::ModifyResource_Implementation(EResourceType ResourceType, int32 TeamId, float Amount){
	
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode) return;
	
	GameMode->ModifyResource(ResourceType, TeamId, Amount);
}

void AControllerBase::SetControlerTeamId_Implementation(int Id)
{
	SelectableTeamId = Id;
}

void AControllerBase::SetControlerDefaultWaypoint_Implementation(AWaypoint* Waypoint)
{
	if(Waypoint)
		DefaultWaypoint = Waypoint;
}

void AControllerBase::LoadLevel_Implementation(const FString& SlotName)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			//SelectedUnits[i]->LoadLevelDataAndAttributes(SlotName);
			SelectedUnits[i]->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AControllerBase::SaveLevel_Implementation(const FString& SlotName)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			//SelectedUnits[i]->SaveLevelDataAndAttributes(SlotName);
			SelectedUnits[i]->SaveAbilityAndLevelData(SlotName);
		}
	}
}

void AControllerBase::LevelUp_Implementation()
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			SelectedUnits[i]->LevelUp();
		}
	}
}



void AControllerBase::ResetTalents_Implementation()
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			SelectedUnits[i]->ResetTalents();
		}
	}
}

void AControllerBase::HandleInvestment_Implementation(int32 InvestmentState)
{
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i] && IsLocalController())
		{
			switch (InvestmentState)
			{
			case 0:
				{
					SelectedUnits[i]->InvestPointIntoStamina();
				}
				break;
			case 1:
				{
					SelectedUnits[i]->InvestPointIntoAttackPower();
				}
				break;
			case 2:
				{
					SelectedUnits[i]->InvestPointIntoWillPower();
				}
				break;
			case 3:
				{
					SelectedUnits[i]->InvestPointIntoHaste();
				}
				break;
			case 4:
				{
					SelectedUnits[i]->InvestPointIntoArmor();
				}
				break;
			case 5:
				{
					SelectedUnits[i]->InvestPointIntoMagicResistance();
				}
				break;
			case 6:
				{
					UE_LOG(LogTemp, Warning, TEXT("None"));
				}
				break;
			default:
				break;
				}
		}
	}
}

void AControllerBase::SaveLevelUnit_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			//Unit->SaveLevelDataAndAttributes(SlotName);
			Unit->SaveAbilityAndLevelData(SlotName);
		}
	}
}

void AControllerBase::LoadLevelUnit_Implementation(const int32 UnitIndex, const FString& SlotName)
{

	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if(Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
			//Unit->LoadLevelDataAndAttributes(SlotName);
			Unit->LoadAbilityAndLevelData(SlotName);
	}


}

void AControllerBase::LevelUpUnit_Implementation(const int32 UnitIndex)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->LevelUp();
		}
	}
}

void AControllerBase::ResetTalentsUnit_Implementation(const int32 UnitIndex)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->ResetTalents();
		}
	}
}

void AControllerBase::HandleInvestmentUnit_Implementation(const int32 UnitIndex, int32 InvestmentState)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			switch (InvestmentState)
			{
			case 0:
				{
					Unit->InvestPointIntoStamina();
				}
				break;
			case 1:
				{
					Unit->InvestPointIntoAttackPower();
				}
				break;
			case 2:
				{
					Unit->InvestPointIntoWillPower();
				}
				break;
			case 3:
				{
					Unit->InvestPointIntoHaste();
				}
				break;
			case 4:
				{
					Unit->InvestPointIntoArmor();
				}
				break;
			case 5:
				{
					Unit->InvestPointIntoMagicResistance();
				}
				break;
			case 6:
				{
					UE_LOG(LogTemp, Warning, TEXT("None"));
				}
				break;
			default:
				break;
			}
		}
	}
}


void AControllerBase::SpendAbilityPoints_Implementation(EGASAbilityInputID AbilityID, int Ability, const int32 UnitIndex)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->SpendAbilityPoints(AbilityID, Ability);
		}
	}
}

void AControllerBase::ResetAbility_Implementation(const int32 UnitIndex)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->ResetAbility();
		}
	}
}

void AControllerBase::SaveAbility_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->SaveAbilityAndLevelData(SlotName);
		}
	}
}

void AControllerBase::LoadAbility_Implementation(const int32 UnitIndex, const FString& SlotName)
{
	for (int32 i = 0; i < HUDBase->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(HUDBase->AllUnits[i]);
		if (Unit && Unit->UnitIndex == UnitIndex && IsLocalController())
		{
			Unit->LoadAbilityAndLevelData(SlotName);
		}
	}
}

void AControllerBase::AddWorkerToResource_Implementation(EResourceType ResourceType, int TeamId)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode)
	{
		GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, 1); // Assuming this function exists in GameMode
	}
}

void AControllerBase::RemoveWorkerFromResource_Implementation(EResourceType ResourceType, int TeamId)
{
	AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode)
	{
		GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, -1); // Assuming this function exists in GameMode
	}
}