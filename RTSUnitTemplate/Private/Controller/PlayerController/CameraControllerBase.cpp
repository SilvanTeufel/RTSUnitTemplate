// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Controller/PlayerController/CameraControllerBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Characters/Unit/UnitBase.h" // Include UnitBase for the RPC


bool ACameraControllerBase::Server_UpdateCameraUnitMovement_Validate(AUnitBase* Unit, const FVector& TargetLocation)
{
	return Unit != nullptr;
}

void ACameraControllerBase::Server_UpdateCameraUnitMovement_Implementation(AUnitBase* Unit, const FVector& TargetLocation)
{
	if (!Unit || !Unit->Attributes) return;


	if (Unit->GetUnitState() != UnitData::Casting)
	{

        	
		if (CameraUnitWithTag)
		{
			bool bNavMod = false;
			FVector ValidatedLocation = TraceRunLocation(TargetLocation, bNavMod); // Projiziert die Position auf das Navmesh/den Boden.

			if (bNavMod)
			{
				// Position ist ungültig, keine Bewegung ausführen.
				return;
			}

			//DrawDebugCircle(GetWorld(), ValidatedLocation, 40.f, 16, FColor::Green, false, 0.5f);
			const float Speed = Unit->Attributes->GetBaseRunSpeed();
			CorrectSetUnitMoveTarget(GetWorld(), Unit, ValidatedLocation, Speed, 40.f);
		}
	}
}


void ACameraControllerBase::Server_TravelToMap_Implementation(const FString& MapName)
{
	// This code now runs on the SERVER
	if (HasAuthority())
	{
		if (!MapName.IsEmpty())
		{
			GetWorld()->ServerTravel(MapName);
		}
	}
}
#include "AIController.h"
#include "Actors/AutoCamWaypoint.h"
#include "Engine/GameViewportClient.h" // Include the header for UGameViewportClient
#include "Engine/Engine.h"      
#include "Kismet/GameplayStatics.h"


ACameraControllerBase::ACameraControllerBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACameraControllerBase::BeginPlay()
{
	Super::BeginPlay();

	HUDBase = Cast<APathProviderHUD>(GetHUD());
	CameraBase = Cast<ACameraBase>(GetPawn());
	SetActorTickEnabled(true);
	
	if(CameraBase) GetViewPortScreenSizes(CameraBase->GetViewPortScreenSizesState);
	
	GetAutoCamWaypoints();
}


void ACameraControllerBase::SetCameraUnitWithTag_Implementation(FGameplayTag Tag, int TeamId)
{
	
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if (GameMode)
	{
		for (int32 i = 0; i < GameMode->AllUnits.Num(); i++)
		{
			AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
			
			if (Unit && Unit->UnitTags.HasTagExact(Tag) && Unit->TeamId == TeamId)
			{
				ServerSetCameraUnit(Unit, TeamId);
				ClientSetCameraUnit(Unit, TeamId);
				
			}
		}
	}
}

void ACameraControllerBase::ServerSetCameraUnit_Implementation(AUnitBase* CameraUnit, int TeamId)
{
	if (TeamId != SelectableTeamId) return;
	
	CameraUnitWithTag = CameraUnit;
				
	CameraBase = Cast<ACameraBase>(GetPawn());

	if(CameraBase)
		CameraBase->SetCameraState(CameraData::LockOnCharacterWithTag);
}

void ACameraControllerBase::Multi_SetCameraOnly_Implementation()
{
	CameraBase = Cast<ACameraBase>(GetPawn());
}

void ACameraControllerBase::ClientSetCameraUnit_Implementation(AUnitBase* CameraUnit, int TeamId)
{
	if (GetNetMode() == NM_Client) 	UE_LOG(LogTemp, Log, TEXT("!!!!EXECUTED ON CLIENT!!!!!!!!!!"));
	
	if (TeamId != SelectableTeamId) return;


	if (GetNetMode() == NM_Client) 	UE_LOG(LogTemp, Log, TEXT("!!!!EXECUTED ON CLIENT!2222!!!!!!!!!"));
	CameraUnitWithTag = CameraUnit;
				
	CameraBase = Cast<ACameraBase>(GetPawn());

	if(CameraBase)
		CameraBase->SetCameraState(CameraData::LockOnCharacterWithTag);
}

void ACameraControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent(); 
}

void ACameraControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CheckSpeakingUnits();
	RotateCam(DeltaSeconds);
	CameraBaseMachine(DeltaSeconds);
}

void ACameraControllerBase::MoveCamToLocation(ACameraBase* Camera, const FVector& DestinationLocation)
{
	if (!Camera || !Camera->GetCharacterMovement() || !Camera->GetController())
	{
		return;
	}

	// Check if we have a valid AI controller for the unit
	AAIController* AIController = Cast<AAIController>(Camera->GetController());
	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("No AI-Controller found"));
		return;
	}
	
}

bool ACameraControllerBase::CheckSpeakingUnits()
{
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	
	if(GameMode)
	for (int32 i = 0; i < GameMode->SpeakingUnits.Num(); i++)
	{
		if(GameMode->SpeakingUnits[i]->LockCamOnUnit)
		{
			SpeakingUnit = GameMode->SpeakingUnits[i];
			SetCameraState(CameraData::LockOnSpeaking);
			return true;
		}
	}
	SpeakingUnit = nullptr;
	return false;
}

void ACameraControllerBase::SetCameraState(TEnumAsByte<CameraData::CameraState> NewCameraState)
{
	
	CameraBase->SetCameraState(NewCameraState);
}

void ACameraControllerBase::GetViewPortScreenSizes(int x)
{
	switch (x)
	{
	case 1:
		GetViewportSize(CameraBase->ScreenSizeX, CameraBase->ScreenSizeY);
		break;
	case 2:
		if (GEngine && GEngine->GameViewport)
		{
			FViewport* Viewport = GEngine->GameViewport->Viewport;
			FIntPoint Size = Viewport->GetSizeXY();
			CameraBase->ScreenSizeX = Size.X;
			CameraBase->ScreenSizeY = Size.Y;
		}
		break;
	}
}

FVector ACameraControllerBase::GetCameraPanDirection() {
	float MousePosX = 0;
	float MousePosY = 0;
	float CamDirectionX = 0;
	float CamDirectionY = 0;

	GetMousePosition(MousePosX, MousePosY);

	const float CosYaw = FMath::Cos(CameraBase->SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(CameraBase->SpringArmRotator.Yaw*PI/180);
	
	if (MousePosX <= CameraBase->Margin)
	{
		CamDirectionY = -CosYaw;
		CamDirectionX = SinYaw;
	}
	if (MousePosY <= CameraBase->Margin)
	{
		CamDirectionX = CosYaw;
		CamDirectionY = SinYaw;
	}
	if (MousePosX >= CameraBase->ScreenSizeX - CameraBase->Margin)
	{
		CamDirectionY = CosYaw;
		CamDirectionX = -SinYaw;
	}
	if (MousePosY >= CameraBase->ScreenSizeY - CameraBase->Margin)
	{
		CamDirectionX = -CosYaw;
		CamDirectionY = -SinYaw;
	}
	
	return FVector(CamDirectionX, CamDirectionY, 0);
}

void ACameraControllerBase::SetCameraZDistance(int Index)
{
	if(SelectedUnits.Num() && SelectedUnits[Index])
	{
		float Distance = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);

		if(Distance >= 50.f)
		{
			CameraBase->CameraDistanceToCharacter = Distance;
			
		}else
		{
			CameraBase->CameraDistanceToCharacter = 50.f;
		}
	}
}

void ACameraControllerBase::RotateCam(float DeltaTime)
{
	if(!MiddleMouseIsPressed) return;
	
	//FHitResult Hit;
	//GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

	if(CameraBase)
	{
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			CameraBase->RotateFree(FVector(MouseX, MouseY, 0.f));
		}
	}
}

FVector ACameraControllerBase::CalculateUnitsAverage(float DeltaTime) {
	
	FVector SumPosition(0, 0, 0);
	int32 UnitCount = 0;
	TArray <AActor*> Units;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnitBase::StaticClass(), Units);
	
	
	int32 UnitsInRangeCount = 0;
	// Count Units within the specified Radius
	
	
	for (AActor* Unit : Units) {
		if (Unit) {
			FVector UnitLocation = Unit->GetActorLocation();
			AUnitBase* UnitBase = Cast<AUnitBase>(Unit);
			if(AutoCamPlayerOnly)
			{
				if(UnitBase->IsPlayer)
					if (FVector::Dist(UnitLocation, OrbitPositions[OrbitRotatorIndex]) <= OrbitRadiuses[OrbitRotatorIndex]) {
						UnitsInRangeCount++;
						SumPosition += UnitLocation;
						UnitCount++;
					}

			}
			else
			{
				if (FVector::Dist(UnitLocation, OrbitPositions[OrbitRotatorIndex]) <= OrbitRadiuses[OrbitRotatorIndex]) {
					UnitsInRangeCount++;
					SumPosition += UnitLocation;
					UnitCount++;
				}
			}
		}
	}
	float UnitTimePart = UnitsInRangeCount*UnitCountOrbitTimeMultiplyer;
	float MaxTime = OrbitTimes[OrbitRotatorIndex] + UnitTimePart;

	if(OrbitLocationControlTimer >= MaxTime)
	{
		OrbitRotatorIndex++;
		OrbitLocationControlTimer = 0.f;
	}
	UnitCountInRange = UnitsInRangeCount;
	
	if(OrbitRotatorIndex >= OrbitPositions.Num())
		OrbitRotatorIndex = 0;

	
	Units.Empty();

	if (UnitCount == 0) return OrbitPositions[OrbitRotatorIndex];
	return SumPosition / UnitCount;
}

void ACameraControllerBase::GetAutoCamWaypoints()
{
	TArray<AActor*> Waypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAutoCamWaypoint::StaticClass(), Waypoints);

	//TArray<AAutoCamWaypoint*> Waypoints;
	AAutoCamWaypoint* StartWaypoint = nullptr;
    
	// Find the first Waypoint that has a NextWaypoint assigned
	for (AActor* ActorWaypoint : Waypoints)
	{
		AAutoCamWaypoint* Waypoint = Cast<AAutoCamWaypoint>(ActorWaypoint);
		
		if (Waypoint && Waypoint->NextWaypoint)
		{
			StartWaypoint = Waypoint;
			break;
		}
	}
    
	// If a valid StartWaypoint is found, setup the orbit positions
	if (StartWaypoint)
	{
		OrbitPositions.Empty(); // Clear existing waypoints
		OrbitTimes.Empty();
		
		AAutoCamWaypoint* CurrentWaypoint = StartWaypoint;
		do
		{
			OrbitPositions.Add(CurrentWaypoint->GetActorLocation());
			OrbitTimes.Add(CurrentWaypoint->OrbitTime);
			CurrentWaypoint = CurrentWaypoint->NextWaypoint;
		} 
		while (CurrentWaypoint && CurrentWaypoint != StartWaypoint); // Continue until loop completes or returns to start
	}
	
}

void ACameraControllerBase::SetCameraAveragePosition(ACameraBase* Camera, float DeltaTime) {

	FVector CameraPosition = CalculateUnitsAverage(DeltaTime);

	Camera->SetActorLocation(FVector(CameraPosition.X, CameraPosition.Y, Camera->GetActorLocation().Z)); // Z-Koordinate bleibt unverändert
}

void ACameraControllerBase::CameraBaseMachine(float DeltaTime)
{
	if(!CameraBase) return;
	
	if (CameraBase && SelectedUnits.Num() && LockCameraToUnit)
	{
		CameraBase->LockOnUnit(SelectedUnits[0]);
		LockCameraToCharacter = true;
	}

	if(CameraBase->BlockControls) return;
	
	FVector PanDirection = GetCameraPanDirection();
	
	if(CameraBase)
	{
		switch (CameraBase->GetCameraState())
		{
		case CameraData::UseScreenEdges:
			CameraState_UseScreenEdges();
			break;
		case CameraData::MoveWASD:
			CameraState_MoveWASD(DeltaTime);
			break;
		case CameraData::ZoomIn:
			CameraState_ZoomIn();
			break;
		case CameraData::ZoomOut:
			CameraState_ZoomOut();
			break;
		case CameraData::ScrollZoomIn:
			CameraState_ScrollZoomIn();
			break;
		case CameraData::ScrollZoomOut:
			CameraState_ScrollZoomOut();
			break;
		case CameraData::ZoomOutPosition:
			CameraState_ZoomOutPosition();
			break;
		case CameraData::ZoomInPosition:
			CameraState_ZoomInPosition();
			break;
		case CameraData::HoldRotateLeft:
			CameraState_HoldRotateLeft();
			break;
		case CameraData::HoldRotateRight:
			CameraState_HoldRotateRight();
			break;
		case CameraData::RotateLeft:
			CameraState_RotateLeft();
			break;
		case CameraData::RotateRight:
			CameraState_RotateRight();
			break;
		case CameraData::LockOnCharacter:
			CameraState_LockOnCharacter();
			break;
		case CameraData::LockOnCharacterWithTag:
			CameraState_LockOnCharacterWithTag(DeltaTime);
			break;
		case CameraData::LockOnSpeaking:
			CameraState_LockOnSpeaking();
			break;
		case CameraData::ZoomToNormalPosition:
			CameraState_ZoomToNormalPosition();
			break;
		case CameraData::ZoomToThirdPerson:
			CameraState_ZoomToThirdPerson();
			break;
		case CameraData::ThirdPerson:
			CameraState_ThirdPerson();
			break;
		case CameraData::RotateToStart:
			CameraState_RotateToStart();
			break;
		case CameraData::MoveToPosition:
			CameraState_MoveToPosition(DeltaTime);
			break;
		case CameraData::OrbitAtPosition:
			CameraState_OrbitAtPosition();
			break;
		case CameraData::MoveToClick:
			CameraState_MoveToClick(DeltaTime);
			break;
		case CameraData::LockOnActor:
			CameraState_LockOnActor();
			break;
		case CameraData::OrbitAndMove:
			CameraState_OrbitAndMove(DeltaTime);
			break;
		default:
			CameraBase->SetCameraState(CameraData::UseScreenEdges);
			break;
		}
	}
}

void ACameraControllerBase::CameraState_UseScreenEdges()
{
	if(!CameraBase->DisableEdgeScrolling)
		CameraBase->PanMoveCamera(GetCameraPanDirection()*CameraBase->EdgeScrollCamSpeed);

	if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState) CameraBase->SetCameraState(CameraData::MoveWASD);
	else if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);

	if (SelectedUnits.Num() == 0 || !SelectedUnits[0])
	{
		return;
	}

	AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
	if (!DraggedWorkArea)
	{
		return;
	}

	if (IsLocalController())
	{
		CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
	}
	if (IsLocalController() && !HasAuthority())
	{
		Server_ZoomInToPosition(CameraBase->ZoomPosition, FVector::ZeroVector);
	}
}

void ACameraControllerBase::CameraState_MoveWASD(float DeltaTime)
{
	LockCameraToCharacter = false;

	FVector MoveDirection = FVector::ZeroVector;

	if(WIsPressedState == 1)
	{
		MoveDirection.X += 1.0f;
	}
	if(SIsPressedState == 1)
	{
		MoveDirection.X -= 1.0f;
	}
	if(AIsPressedState == 1)
	{
		MoveDirection.Y -= 1.0f;
	}
	if(DIsPressedState == 1)
	{
		MoveDirection.Y += 1.0f;
	}

	if (!MoveDirection.IsNearlyZero())
	{
		if (IsLocalController() && CameraBase)
		{
			CameraBase->MoveInDirection(MoveDirection, DeltaTime);

			// Sende die neue Position zum Server (nicht die Bewegung!)
			// Der Server speichert sie nur, überschreibt aber nicht die Client-Position
			if (!HasAuthority())
			{
				Server_SyncCameraPosition(CameraBase->GetActorLocation());
			}
		}
	}

	if(CamIsRotatingLeft)
	{
		if (IsLocalController() && CameraBase)
		{
			CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, !CamIsRotatingLeft);
		}
	}

	if(CamIsRotatingRight)
	{
		if (IsLocalController() && CameraBase)
		{
			CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, !CamIsRotatingRight);
		}
	}

	if(MoveDirection.IsNearlyZero() && CameraBase->CurrentRotationValue == 0.0f)
	{
		CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}

	if (SelectedUnits.Num() == 0 || !SelectedUnits[0])
	{
		return;
	}

	AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
	if (!DraggedWorkArea)
	{
		return;
	}

	if (IsLocalController())
	{
		CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
	}
	if (IsLocalController() && !HasAuthority())
	{
		Server_ZoomInToPosition(CameraBase->ZoomPosition, FVector::ZeroVector);
	}
}

void ACameraControllerBase::CameraState_ZoomIn()
{
	if(CamIsZoomingInState == 1)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f);
		}
	}
	if(CamIsZoomingInState == 2)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f, true);
		}
	}
	if(CamIsZoomingInState != 1 && CameraBase->CurrentCamSpeed.Z == 0.f) CamIsZoomingInState = 0;

	if(CamIsZoomingInState != 1 && CamIsZoomingOutState == 1)SetCameraState(CameraData::ZoomOut);

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
	else if(!CamIsZoomingInState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
}

void ACameraControllerBase::CameraState_ZoomOut()
{
	if(CamIsZoomingOutState == 1)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f);
		}
	}
	if(CamIsZoomingOutState == 2)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f, true);
		}
	}
	if(CamIsZoomingOutState != 1 && CameraBase->CurrentCamSpeed.Z == 0.f) CamIsZoomingOutState = 0;

	if(CamIsZoomingOutState != 1 && CamIsZoomingInState == 1)SetCameraState(CameraData::ZoomIn);

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
	else if(!CamIsZoomingOutState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
}

void ACameraControllerBase::HandleScrollZoomIn()
{
	if(ScrollZoomCount > 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f);
			CameraBase->RotateSpringArm(false);
		}

		SetCameraZDistance(0);
	}
	if(ScrollZoomCount <= 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomIn(1.f, true);
			CameraBase->RotateSpringArm(false);
		}

		SetCameraZDistance(0);
	}

	if(ScrollZoomCount > 0.f)
		ScrollZoomCount -= 0.25f;
}

void ACameraControllerBase::HandleScrollZoomOut()
{
	if(ScrollZoomCount < 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f);
			CameraBase->RotateSpringArm(true);
		}

		SetCameraZDistance(0);
	}
	if(ScrollZoomCount >= 0.f)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOut(1.f, true);
			CameraBase->RotateSpringArm(true);
		}

		SetCameraZDistance(0);
	}

	if(ScrollZoomCount < 0.f)
		ScrollZoomCount += 0.25f;
}

void ACameraControllerBase::CameraState_ScrollZoomIn()
{
	HandleScrollZoomIn();

	if(ScrollZoomCount < 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		CameraBase->SetCameraState(CameraData::ScrollZoomOut);
	}else if(ScrollZoomCount == 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else if(!CamIsZoomingInState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}

	if(LockCameraToCharacter)
	{
		LockCamToCharacter(0);
	}
}

void ACameraControllerBase::CameraState_ScrollZoomOut()
{
	HandleScrollZoomOut();

	if(ScrollZoomCount > 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		CameraBase->SetCameraState(CameraData::ScrollZoomIn);
	}else if(ScrollZoomCount == 0.f && CameraBase->CurrentCamSpeed.Z == 0.f)
	{
		if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else if(!CamIsZoomingOutState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}

	if(LockCameraToCharacter)
	{
		LockCamToCharacter(0);
	}
}

void ACameraControllerBase::CameraState_ZoomOutPosition()
{
	ZoomOutToPosition = true;

	if (IsLocalController())
	{
		CameraBase->ZoomOutToPosition(CameraBase->ZoomOutPosition);
	}
}

void ACameraControllerBase::CameraState_ZoomInPosition()
{
	ZoomOutToPosition = false;
	ZoomInToPosition = true;

	bool bZoomComplete = false;
	if (IsLocalController())
	{
		bZoomComplete = CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
	}

	if(bZoomComplete)
	{
		SetCameraZDistance(0);
		ZoomInToPosition = false;
		if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_HoldRotateLeft()
{
	CamIsRotatingRight = false;
	if(LockCameraToCharacter)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::LockOnCharacter);
	}

	if (IsLocalController() && CameraBase)
	{
		CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, !CamIsRotatingLeft);
	}

	if(!CamIsRotatingLeft && CameraBase->CurrentRotationValue == 0.f)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_HoldRotateRight()
{
	CamIsRotatingLeft = false;

	if(LockCameraToCharacter)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::LockOnCharacter);
	}

	if (IsLocalController() && CameraBase)
	{
		CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, !CamIsRotatingRight);
	}

	if(!CamIsRotatingRight  && CameraBase->CurrentRotationValue == 0.f)
	{
		CameraBase->CurrentRotationValue = 0.f;
		CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_RotateLeft()
{
	CamIsRotatingRight = false;
	CamIsRotatingLeft = true;

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);

	bool bRotationComplete = false;
	if (IsLocalController() && CameraBase)
	{
		bRotationComplete = CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, false);
	}

	if(bRotationComplete)
	{
		CamIsRotatingLeft = false;
		if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_RotateRight()
{
	CamIsRotatingLeft = false;
	CamIsRotatingRight = true;

	if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);

	bool bRotationComplete = false;
	if (IsLocalController() && CameraBase)
	{
		bRotationComplete = CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
	}

	if(bRotationComplete)
	{
		CamIsRotatingRight = false;
		if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraControllerBase::CameraState_LockOnCharacter()
{
	LockCamToCharacter(0);
}

void ACameraControllerBase::CameraState_LockOnCharacterWithTag(float DeltaTime)
{
	LockCamToCharacterWithTag(DeltaTime);
}

void ACameraControllerBase::CameraState_LockOnSpeaking()
{
	if(SpeakingUnit)
		LockCamToSpecificUnit(SpeakingUnit);
	else
		SetCameraState(CameraData::ZoomToNormalPosition);
}

void ACameraControllerBase::CameraState_ZoomToNormalPosition()
{
	bool bZoomComplete = false;
	if (IsLocalController())
	{
		bZoomComplete = CameraBase->ZoomOutToPosition(CameraBase->ZoomPosition);
	}

	if(bZoomComplete)
	{
		bool bRotationComplete = false;

		if (IsLocalController() && CameraBase)
		{
			bRotationComplete = CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
		}

		if(bRotationComplete)
		{
			CamIsRotatingRight = false;
			CamIsRotatingLeft = false;
			if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
			else CameraBase->SetCameraState(CameraData::LockOnCharacter);
		}
	};
}

void ACameraControllerBase::CameraState_ZoomToThirdPerson()
{
	if( SelectedUnits.Num())
	{
		FVector SelectedActorLocation = SelectedUnits[0]->GetActorLocation();

		CameraBase->LockOnUnit(SelectedUnits[0]);
		if (!CameraBase->IsCameraInAngle())
		{
			if (IsLocalController() && CameraBase)
			{
				CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
			}
		}
		else
		{
			bool bZoomComplete = false;
			if (IsLocalController())
			{
				bZoomComplete = CameraBase->ZoomInToThirdPerson(SelectedActorLocation);
			}

			if(bZoomComplete)
			{
				LockCameraToCharacter = false;
				CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
				CameraBase->SetCameraState(CameraData::ThirdPerson);
			}
		}

	}
}

void ACameraControllerBase::CameraState_ThirdPerson()
{
	if( SelectedUnits.Num())
	{
		float YawActor = SelectedUnits[0]->GetActorRotation().Yaw;
		float YawCamera = CameraBase->GetActorRotation().Yaw;

		CameraBase->LockOnUnit(SelectedUnits[0]);

		if(YawCamera-YawActor < -90)
			CameraBase->RotateCamRightTo(YawActor, CameraBase->AddCamRotation/3);
		else if(YawCamera-YawActor > 90)
			CameraBase->RotateCamRightTo(YawActor, CameraBase->AddCamRotation/3);
		else if(YawCamera-YawActor < -25)
			CameraBase->RotateCamLeftTo(YawActor, CameraBase->AddCamRotation/3);
		else if(YawCamera-YawActor > 25)
			CameraBase->RotateCamRightTo(YawActor, CameraBase->AddCamRotation/3);
	}
}

void ACameraControllerBase::CameraState_RotateToStart()
{
	if (FMath::IsNearlyEqual(CameraBase->SpringArmRotator.Yaw, CameraBase->CameraAngles[0], CameraBase->RotationIncreaser))
		CameraBase->SetCameraState(CameraData::MoveToPosition);

	if (IsLocalController() && CameraBase)
	{
		CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation/3, false);
	}
}

void ACameraControllerBase::CameraState_MoveToPosition(float DeltaTime)
{
	MoveCamToPosition(DeltaTime, CameraBase->OrbitLocation);
}

void ACameraControllerBase::CameraState_OrbitAtPosition()
{
	CamIsRotatingLeft = true;
	CameraBase->OrbitCamLeft(CameraBase->OrbitSpeed);

	if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState)
	{
		CamIsRotatingLeft = false;
		CameraBase->SetCameraState(CameraData::MoveWASD);
	}
}

void ACameraControllerBase::CameraState_MoveToClick(float DeltaTime)
{
	if(ClickedActor)
		MoveCamToClick(DeltaTime, ClickedActor->GetActorLocation());
}

void ACameraControllerBase::CameraState_LockOnActor()
{
	CameraBase->LockOnActor(ClickedActor);
}

void ACameraControllerBase::CameraState_OrbitAndMove(float DeltaTime)
{
	CamIsRotatingLeft = true;
	CameraBase->OrbitCamLeft(CameraBase->OrbitSpeed);

	CalcControlTimer += DeltaTime;
	OrbitLocationControlTimer += DeltaTime;


	if(CalcControlTimer >= OrbitAndMovePauseTime)
	{
		FVector CameraPosition = CalculateUnitsAverage(DeltaTime);
		CameraBase->OrbitLocation = FVector(CameraPosition.X, CameraPosition.Y, CameraBase->GetActorLocation().Z);
		CalcControlTimer = 0.f;
	}

	MoveCam(DeltaTime, CameraBase->OrbitLocation);

	if(UnitCountInRange >= UnitCountToZoomOut)
	{
		if (IsLocalController())
		{
			CameraBase->ZoomOutAutoCam(CameraBase->ZoomPosition+UnitZoomScaler*UnitCountInRange);
		}
	}
	else
	{
		if (IsLocalController())
		{
			CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
		}
	}

	if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState)
	{
		CamIsRotatingLeft = false;
		CameraBase->SetCameraState(CameraData::MoveWASD);
	}
}



void ACameraControllerBase::OrbitAtLocation(FVector Destination, float OrbitSpeed)
{
	CameraBase->OrbitLocation = Destination;
	CameraBase->OrbitSpeed = OrbitSpeed;
	CameraBase->OrbitRotationValue = 0.f;
	AIsPressedState = 0;
	DIsPressedState = 0;
	WIsPressedState = 0;
	SIsPressedState = 0;
	CameraBase->SetCameraState(CameraData::RotateToStart);
}

void ACameraControllerBase::Server_MoveCamToPosition_Implementation(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	const FVector CamLocation = CameraBase->GetActorLocation();

	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

	const float Distance = FVector::Distance(CamLocation, Destination);
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

	if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
		CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
	else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f;

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

	if (Distance <= 50.f) {
		CameraBase->SetCameraState(CameraData::OrbitAtPosition);
	}
}

void ACameraControllerBase::Server_SetCameraLocation_Implementation(FVector NewLocation)
{
	if (CameraBase)
	{
		CameraBase->SetActorLocation(NewLocation);
	}
}

void ACameraControllerBase::MoveCamToPosition(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	// Client-Side Prediction - Lokale Ausführung für sofortige Reaktion
	if (IsLocalController())
	{
		const FVector CamLocation = CameraBase->GetActorLocation();

		Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

		const float Distance = FVector::Distance(CamLocation, Destination);
		const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

		if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
			CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
		else
			CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f;

		CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

		if (Distance <= 50.f) {
			CameraBase->SetCameraState(CameraData::OrbitAtPosition);
		}
	}

	// Server-RPC (nur wenn Client)
	if (IsLocalController() && !HasAuthority())
	{
		Server_MoveCamToPosition(DeltaSeconds, Destination);
	}
}

void ACameraControllerBase::Server_MoveCamToClick_Implementation(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	const FVector CamLocation = CameraBase->GetActorLocation();

	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

	const float Distance = FVector::Distance(CamLocation, Destination);
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

	CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 2000.0f? 200.f : 0.f;

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

	if (Distance <= 50.f) {
		CameraBase->SetCameraState(CameraData::LockOnActor);
	}
}

void ACameraControllerBase::MoveCamToClick(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	// Client-Side Prediction
	if (IsLocalController())
	{
		const FVector CamLocation = CameraBase->GetActorLocation();

		Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

		const float Distance = FVector::Distance(CamLocation, Destination);
		const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 2000.0f? 200.f : 0.f;

		CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

		if (Distance <= 50.f) {
			CameraBase->SetCameraState(CameraData::LockOnActor);
		}
	}

	// Server-RPC (nur wenn Client)
	if (IsLocalController() && !HasAuthority())
	{
		Server_MoveCamToClick(DeltaSeconds, Destination);
	}
}

void ACameraControllerBase::Server_MoveCam_Implementation(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	const FVector CamLocation = CameraBase->GetActorLocation();

	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

	const float Distance = FVector::Distance(CamLocation, Destination);
	if (Distance <= 50.f) return;

	const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

	if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
		CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
	else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f;

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);
}

void ACameraControllerBase::MoveCam(float DeltaSeconds, FVector Destination)
{
	if (!CameraBase)
	{
		return;
	}

	// Client-Side Prediction
	if (IsLocalController())
	{
		const FVector CamLocation = CameraBase->GetActorLocation();

		Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);

		const float Distance = FVector::Distance(CamLocation, Destination);
		if (Distance <= 50.f) return;

		const FVector ADirection = (Destination - CamLocation).GetSafeNormal();

		if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
			CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
		else
			CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f;

		CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);
	}

	// Server-RPC (nur wenn Client)
	if (IsLocalController() && !HasAuthority())
	{
		Server_MoveCam(DeltaSeconds, Destination);
	}
}

void ACameraControllerBase::ToggleLockCamToCharacter()
{
	if(IsCtrlPressed)
	{
		LockCameraToCharacter = !LockCameraToCharacter;
	
		if(LockCameraToCharacter)
			CameraBase->SetCameraState(CameraData::LockOnCharacter);
		else
		{
			CameraBase->SetCameraState(CameraData::UseScreenEdges);
		}
	}
}
void ACameraControllerBase::UnlockCamFromCharacter()
{
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit);

	if (Hit.bBlockingHit)
	{
		AUnitBase* CharacterFound = Cast<AUnitBase>(Hit.GetActor());

		if(!CharacterFound){
			CameraBase->SetCameraState(CameraData::UseScreenEdges);
			LockCameraToCharacter = false;
		}
	}
	
}

void ACameraControllerBase::LockCamToSpecificUnit(AUnitBase* SUnit)
{
	ASpeakingUnit* Unit = Cast<ASpeakingUnit>(SUnit);
	
	if( Unit)
	{
		FVector SelectedActorLocation = Unit->GetActorLocation();
		
		CameraBase->LockOnUnit(Unit);

		CamIsRotatingLeft = true;

		if (IsLocalController() && CameraBase)
		{
			CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotationSpeaking/100, false);
		}

		if(CamIsZoomingInState)
		{
			if (IsLocalController())
			{
				CameraBase->ZoomIn(1.f);
			}
		}
		else if(CamIsZoomingOutState)
		{
			if (IsLocalController())
			{
				CameraBase->ZoomOut(1.f);
			}
		}
		else if(ZoomOutToPosition)
		{
			if (IsLocalController())
			{
				CameraBase->ZoomOutToPosition(CameraBase->ZoomOutPosition, SelectedActorLocation);
			}
		}
		else if(ZoomInToPosition)
		{
			bool bZoomComplete = false;
			if (IsLocalController())
			{
				bZoomComplete = CameraBase->ZoomInToPosition(CameraBase->ZoomPosition, SelectedActorLocation);
			}

			if(bZoomComplete) ZoomInToPosition = false;
		}
		else if(CameraBase->IsCharacterDistanceTooHigh(Unit->SpeakZoomPosition, SelectedActorLocation))
		{
			if (IsLocalController())
			{
				CameraBase->ZoomInToPosition(Unit->SpeakZoomPosition, SelectedActorLocation);
				CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - Unit->GetActorLocation().Z);
			}
		}

	}else
	{ 
		LockCameraToCharacter = !LockCameraToCharacter;
		CameraBase->SetCameraState(CameraData::ZoomInPosition);
	}
}

void ACameraControllerBase::LockCamToCharacter(int Index)
{
	if( SelectedUnits.Num() && SelectedUnits[Index])
	{
		AUnitBase* TargetUnit = SelectedUnits[Index];
		FVector TargetLocation = TargetUnit->GetActorLocation();

		// --- Interpolation Logic ---
		if(CameraBase)
		{
			const float DeltaTime = GetWorld()->GetDeltaSeconds();
			const float InterpSpeed = 5.0f;

			FVector DesiredCameraLocation = FVector(TargetLocation.X, TargetLocation.Y, CameraBase->GetActorLocation().Z);

			FVector NewCameraLocation = FMath::VInterpTo(CameraBase->GetActorLocation(), DesiredCameraLocation, DeltaTime, InterpSpeed);

			// Client-Side Prediction
			if (IsLocalController())
			{
				CameraBase->SetActorLocation(NewCameraLocation);
			}

			// Server-RPC (nur wenn Client)
			if (IsLocalController() && !HasAuthority())
			{
				Server_SetCameraLocation(NewCameraLocation);
			}
		}
		// --- End Interpolation Logic ---


		if(ScrollZoomCount > 0.f)
		{
			CameraBase->SetCameraState(CameraData::ScrollZoomIn);
		}else if(ScrollZoomCount < 0.f)
		{
			CameraBase->SetCameraState(CameraData::ScrollZoomOut);
		}

		if(RotateBehindCharacterIfLocked)
		{
			float CameraYaw = FMath::Fmod(static_cast<float>(CameraBase->SpringArmRotator.Yaw) + 360.f, 360.f);
			float ActorYaw = FMath::Fmod(static_cast<float>(SelectedUnits[Index]->GetActorRotation().Yaw) + 360.f, 360.f);
			float DeltaYaw = FMath::Fmod(FMath::Fmod(static_cast<float>(ActorYaw + 180.f), 360.f) - CameraYaw + 540.f, 360.f) - 180.f;

			if(!FMath::IsNearlyEqual(CameraYaw, ActorYaw, 10.f))
			{
				float RotationDirection = (DeltaYaw > 0) ? -1.0f : 1.0f;

				if (IsLocalController() && CameraBase)
				{
					CameraBase->RotateCamera(RotationDirection, CameraBase->AddCamRotation*2, false);
				}
			}
		}

		if(CamIsRotatingRight)
		{
			CamIsRotatingLeft = false;
			if (IsLocalController() && CameraBase)
			{
				CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
			}
		}

		if(CamIsRotatingLeft)
		{
			CamIsRotatingRight = false;
			if (IsLocalController() && CameraBase)
			{
				CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, false);
			}
		}
	}else
	{
		LockCameraToCharacter = !LockCameraToCharacter;
		CameraBase->SetCameraState(CameraData::ZoomInPosition);
	}
}

void ACameraControllerBase::MoveAndRotateUnit_Implementation(AUnitBase* Unit, const FVector& Direction, float DeltaTime)
{
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAndRotateUnit: Invalid Unit pointer!"));
		return;
	}

	// 1) Compute the movement speed (just like before).
	const float DefaultSpeedScale = 4.0f;
	float SpeedScale = (Unit->Attributes)
		? Unit->Attributes->GetRunSpeedScale() * 2.f
		: DefaultSpeedScale;

	// 2) Add input vector to the Unit’s movement component.
	//    Make sure AUnitBase::InitializeComponents() has created/assigned UnitMovementComponent.
	if ( UPawnMovementComponent* MovementComponent = Unit->GetMovementComponent())
	{
		// "Direction * SpeedScale" will be interpreted by your movement component.
		MovementComponent->AddInputVector(Direction * SpeedScale);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnitMovementComponent not found on %s"), *Unit->GetName());
	}

	SetUnitState_Multi(Unit, 1);
	Unit->ForceNetUpdate(); 
}

void ACameraControllerBase::LocalMoveAndRotateUnit(AUnitBase* Unit, const FVector& Direction, float DeltaTime)
{
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("LocalMoveAndRotateUnit: Invalid Unit pointer!"));
		return;
	}
    
	// Calculate movement speed, same as in the server RPC.
	const float DefaultSpeedScale = 4.0f;
	float SpeedScale = (Unit->Attributes)
		? Unit->Attributes->GetRunSpeedScale() * 2.f
		: DefaultSpeedScale;
    
	// Add movement input locally
	if (UPawnMovementComponent* MovementComponent = Unit->GetMovementComponent())
	{
		MovementComponent->AddInputVector(Direction * SpeedScale);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LocalMoveAndRotateUnit: UnitMovementComponent not found on %s"), *Unit->GetName());
	}
    
	// Optionally update any local unit state
	SetUnitState_Multi(Unit, 1);
	
}

void ACameraControllerBase::MoveCameraUnit()
{
	// Ensure the CameraUnitWithTag and its attributes are valid
		
	if (!CameraUnitWithTag)
	{
		return;
	}

	if (!IsLocalController()) return;
	
	FVector SpringArmForward = CameraBase->SpringArmRotator.Vector();
	SpringArmForward.Z = 0.f;
	FVector SpringArmRight   = CameraBase->SpringArmRotator.RotateVector(FVector::RightVector);
	SpringArmRight.Z = 0.f;
	
	FVector MoveDirection = FVector::ZeroVector;
	if (WIsPressedState) MoveDirection += SpringArmForward;
	if (SIsPressedState) MoveDirection -= SpringArmForward;
	if (AIsPressedState) MoveDirection -= SpringArmRight;
	if (DIsPressedState) MoveDirection += SpringArmRight;

	bool bHasInput = !MoveDirection.IsNearlyZero();
	//if (bHasInput)
	{
		MoveDirection.Normalize();
	}

	// Set the Pawn's location to the Camera's location
	FVector TargetLocation  = GetPawn()->GetActorLocation();

	// Set the Pawn's rotation to match the Spring Arm's rotation (yaw)
	FRotator TargetRotation = MoveDirection.Rotation();  // CameraBase->SpringArmRotator;
	TargetRotation.Pitch = 0.f; // Optional: Keep the pawn from tilting up or down
	TargetRotation.Roll = 0.f;  // Optional: Keep the pawn from rolling
	

	SetCameraUnitTransform(TargetLocation, TargetRotation);
	// Apply the new transform to the pawn
	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	const float ClientInterpSpeed =5.0f;

	FVector NewLocation = FMath::VInterpTo(CameraUnitWithTag->GetActorLocation(), TargetLocation, DeltaTime, ClientInterpSpeed);
	FRotator NewRotation = FMath::RInterpTo(CameraUnitWithTag->GetActorRotation(), TargetRotation, DeltaTime, ClientInterpSpeed);
    
	CameraUnitWithTag->SetActorTransform(FTransform(NewRotation, NewLocation));
}


void ACameraControllerBase::SetCameraUnitTransform_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	// --- Interpolation ---
	// Get the DeltaTime for frame-rate independent interpolation.
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	// Set an interpolation speed. Higher values will make the movement faster.
	const float InterpSpeed = 5.0f;

	// Smoothly interpolate the pawn's current location to the target location.
	FVector NewPawnLocation = FMath::VInterpTo(
		CameraUnitWithTag->GetActorLocation(), // Current location
		TargetLocation,                         // Target location
		DeltaTime,                              // Time since last frame
		InterpSpeed                             // Interpolation speed
	);

	// Smoothly interpolate the pawn's current rotation to the target rotation.
	FRotator NewPawnRotation = FMath::RInterpTo(
		CameraUnitWithTag->GetActorRotation(), // Current rotation
		TargetRotation,                         // Target rotation
		DeltaTime,                              // Time since last frame
		InterpSpeed                             // Interpolation speed
	);
	// Create the new transform
	FTransform NewTransform = FTransform(NewPawnRotation, NewPawnLocation);
	
	CameraUnitWithTag->SetActorTransform(NewTransform, false, nullptr, ETeleportType::None);
	CameraUnitWithTag->SyncTranslation();
}

void ACameraControllerBase::Server_MoveInDirection_Implementation(FVector Direction, float DeltaTime)
{
	UE_LOG(LogTemp, Warning, TEXT("Server_MoveInDirection called - Direction: %s, DeltaTime: %f"), *Direction.ToString(), DeltaTime);

	if (CameraBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraBase is valid, calling MoveInDirection"));
		CameraBase->MoveInDirection(Direction, DeltaTime);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CameraBase is NULL in Server_MoveInDirection!"));
	}
}

void ACameraControllerBase::Server_SyncCameraPosition_Implementation(FVector NewPosition)
{
	if (CameraBase)
		CameraBase->SetActorLocation(NewPosition);
}

void ACameraControllerBase::Server_RotateCamera_Implementation(float Direction, float Add, bool stopCam)
{
	if (CameraBase)
	{
		CameraBase->RotateCamera(Direction, Add, stopCam);
	}
}

void ACameraControllerBase::Server_RotateSpringArm_Implementation(bool Invert)
{
	if (CameraBase)
	{
		CameraBase->RotateSpringArm(Invert);
	}
}

void ACameraControllerBase::Server_ZoomIn_Implementation(float Value, bool Stop)
{
	if (CameraBase)
	{
		CameraBase->ZoomIn(Value, Stop);
	}
}

void ACameraControllerBase::Server_ZoomOut_Implementation(float Value, bool Stop)
{
	if (CameraBase)
	{
		CameraBase->ZoomOut(Value, Stop);
	}
}

void ACameraControllerBase::Server_ZoomInToPosition_Implementation(float Distance, FVector OptionalLocation)
{
	if (CameraBase)
	{
		CameraBase->ZoomInToPosition(Distance, OptionalLocation);
	}
}

void ACameraControllerBase::Server_ZoomOutToPosition_Implementation(float Distance, FVector OptionalLocation)
{
	if (CameraBase)
	{
		CameraBase->ZoomOutToPosition(Distance, OptionalLocation);
	}
}

void ACameraControllerBase::Server_ZoomInToThirdPerson_Implementation(FVector SelectedActorLocation)
{
	if (CameraBase)
	{
		CameraBase->ZoomInToThirdPerson(SelectedActorLocation);
	}
}

void ACameraControllerBase::Server_ZoomOutAutoCam_Implementation(float Position)
{
	if (CameraBase)
	{
		CameraBase->ZoomOutAutoCam(Position);
	}
}

void ACameraControllerBase::LockCamToCharacterWithTag(float DeltaTime)
{
        if (CameraUnitWithTag)
        {
        	// Calculate movement direction based on input states
        	// Only add direction when state is 1 (active press), not 2 (decelerate)
        	FVector MoveDirection = FVector::ZeroVector;

        	if(WIsPressedState == 1)
        	{
        		MoveDirection.X += 1.0f; // Forward
        	}
        	if(SIsPressedState == 1)
        	{
        		MoveDirection.X -= 1.0f; // Backward
        	}
        	if(AIsPressedState == 1)
        	{
        		MoveDirection.Y -= 1.0f; // Left
        	}
        	if(DIsPressedState == 1)
        	{
        		MoveDirection.Y += 1.0f; // Right
        	}

        	// Execute movement locally for immediate response (Client-Side Prediction)
        	if (!MoveDirection.IsNearlyZero())
        	{
        		// Execute locally for all local controllers (Client and Server)
        		if (IsLocalController() && CameraBase)
        		{
        			CameraBase->MoveInDirection(MoveDirection, DeltaTime);

        			// Sende die neue Kamera-Position zum Server (unreliable für Performance)
        			if (!HasAuthority())
        			{
        				Server_SyncCameraPosition(CameraBase->GetActorLocation());
        			}
        		}

        		const FVector MoveTargetLocation = GetPawn()->GetActorLocation();

        		if (!MoveTargetLocation.Equals(LastCameraUnitMovementLocation, 50.0f))
        		{
        			LastCameraUnitMovementLocation = MoveTargetLocation;
        			Server_UpdateCameraUnitMovement(CameraUnitWithTag, MoveTargetLocation);
        		}
        	}
        	
        	if (ScrollZoomCount > 0.f)
            {

        		HandleScrollZoomIn();
            }
            else if (ScrollZoomCount < 0.f)
            {
            	HandleScrollZoomOut();
            }
        	
     
        	
            if (RotateBehindCharacterIfLocked)
            {
                float CameraYaw = FMath::Fmod(static_cast<float>(CameraBase->SpringArmRotator.Yaw) + 360.f, 360.f);
                float ActorYaw = FMath::Fmod(static_cast<float>(CameraUnitWithTag->GetActorRotation().Yaw) + 360.f, 360.f);
                float DeltaYaw = FMath::Fmod(FMath::Fmod(static_cast<float>(ActorYaw + 180.f), 360.f) - CameraYaw + 540.f, 360.f) - 180.f;

                if (!FMath::IsNearlyEqual(CameraYaw, ActorYaw, 10.f))
                {
                    float RotationDirection = (DeltaYaw > 0) ? -1.0f : 1.0f;

                    if (IsLocalController() && CameraBase)
                    {
                        CameraBase->RotateCamera(RotationDirection, CameraBase->AddCamRotation * 2, false);
                    }
                }
            }

            if (CamIsRotatingRight)
            {
                CamIsRotatingLeft = false;
                if (IsLocalController() && CameraBase)
                {
                    CameraBase->RotateCamera(-1.0f, CameraBase->AddCamRotation, false);
                }
            }

            if (CamIsRotatingLeft)
            {
                CamIsRotatingRight = false;
                if (IsLocalController() && CameraBase)
                {
                    CameraBase->RotateCamera(1.0f, CameraBase->AddCamRotation, false);
                }
            }
          
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unit does not have the required tag."));
        }

}


void ACameraControllerBase::LockZDistanceToCharacter()
{
	if(ZoomInToPosition == false &&
		ZoomOutToPosition == false &&
		CamIsZoomingInState == 0 &&
		CamIsZoomingOutState == 0 &&
		CameraBase &&
		SelectedUnits.Num())
	{
		
		const FVector SelectedActorLocation = SelectedUnits[0]->GetActorLocation();
		const FVector CameraBaseLocation = CameraBase->GetActorLocation();
		
		const float NewCameraDistanceToCharacter = (CameraBaseLocation.Z - SelectedActorLocation.Z);
		float ZChange = CameraBase->CameraDistanceToCharacter - NewCameraDistanceToCharacter;
		
		const float CosYaw = FMath::Cos(CameraBase->SpringArmRotator.Yaw*PI/180);
		const float SinYaw = FMath::Sin(CameraBase->SpringArmRotator.Yaw*PI/180);
		const FVector NewPawnLocation = FVector(SelectedActorLocation.X - CameraBase->CameraDistanceToCharacter * 0.7*CosYaw, SelectedActorLocation.Y - CameraBase->CameraDistanceToCharacter * 0.7*SinYaw, CameraBaseLocation.Z+ZChange);

		// Client-Side Prediction
		if (IsLocalController())
		{
			CameraBase->SetActorLocation(NewPawnLocation);
			CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
		}

		// Server-RPC (nur wenn Client)
		if (IsLocalController() && !HasAuthority())
		{
			Server_SetCameraLocation(NewPawnLocation);
		}
	}
}
