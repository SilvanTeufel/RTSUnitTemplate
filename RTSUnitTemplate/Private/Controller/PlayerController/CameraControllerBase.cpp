// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Controller/PlayerController/CameraControllerBase.h"
#include "AIController.h"
#include "Actors/AutoCamWaypoint.h"
#include "Engine/GameViewportClient.h" // Include the header for UGameViewportClient
#include "Engine/Engine.h"      
#include "Kismet/GameplayStatics.h"


ACameraControllerBase::ACameraControllerBase()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACameraControllerBase::BeginPlay()
{
	Super::BeginPlay();

	HUDBase = Cast<APathProviderHUD>(GetHUD());
	CameraBase = Cast<ACameraBase>(GetPawn());

	if(CameraBase) GetViewPortScreenSizes(CameraBase->GetViewPortScreenSizesState);
	
	GetAutoCamWaypoints();


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

/*
void ACameraControllerBase::GetViewPortScreenSizes(int x)
{
	switch (x)
	{
	case 1:
		{
			GetViewportSize(CameraBase->ScreenSizeX, CameraBase->ScreenSizeY);
		}
		break;
	case 2:
		{
			CameraBase->ScreenSizeX = GSystemResolution.ResX;
			CameraBase->ScreenSizeY = GSystemResolution.ResY;
		}
		break;
	}
}*/


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
			{
				if(!CameraBase->DisableEdgeScrolling)
					CameraBase->PanMoveCamera(PanDirection*CameraBase->EdgeScrollCamSpeed);
				
				if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState) CameraBase->SetCameraState(CameraData::MoveWASD);
				else if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
			}
			break;
		case CameraData::MoveWASD:
			{

				LockCameraToCharacter = false;
				
				if(AIsPressedState == 1) CameraBase->MoveCamToLeft(DeltaTime);
				if(DIsPressedState == 1) CameraBase->MoveCamToRight(DeltaTime);
				if(WIsPressedState == 1) CameraBase->MoveCamToForward(DeltaTime);
				if(SIsPressedState == 1) CameraBase->MoveCamToBackward(DeltaTime);

				if(AIsPressedState == 2) CameraBase->MoveCamToLeft(DeltaTime, true);
				if(DIsPressedState == 2) CameraBase->MoveCamToRight(DeltaTime, true);
				if(WIsPressedState == 2) CameraBase->MoveCamToForward(DeltaTime, true);
				if(SIsPressedState == 2) CameraBase->MoveCamToBackward(DeltaTime, true);

				if(CameraBase->CurrentCamSpeed.X == 0.0f && WIsPressedState == 2) WIsPressedState = 0;
				if(CameraBase->CurrentCamSpeed.X == 0.0f && SIsPressedState == 2) SIsPressedState = 0;
				if(CameraBase->CurrentCamSpeed.Y == 0.0f && DIsPressedState == 2) DIsPressedState = 0;
				if(CameraBase->CurrentCamSpeed.Y == 0.0f && AIsPressedState == 2) AIsPressedState = 0;

				if(CamIsRotatingLeft) CameraBase->RotateCamLeft(CameraBase->AddCamRotation, !CamIsRotatingLeft);
				if(CamIsRotatingRight) CameraBase->RotateCamRight(CameraBase->AddCamRotation, !CamIsRotatingRight);
				
					if(CameraBase->CurrentCamSpeed.X == 0.0f &&
						CameraBase->CurrentCamSpeed.Y == 0.0f &&
						CameraBase->CurrentRotationValue == 0.0f)
					{
						CameraBase->SetCameraState(CameraData::UseScreenEdges);
					}
			

		
			}
			break;
		case CameraData::ZoomIn:
			{
				//UE_LOG(LogTemp, Warning, TEXT("ZoomIn"));
				if(CamIsZoomingInState == 1)CameraBase->ZoomIn(1.f);
				if(CamIsZoomingInState == 2)CameraBase->ZoomIn(1.f, true);
				if(CamIsZoomingInState != 1 && CameraBase->CurrentCamSpeed.Z == 0.f) CamIsZoomingInState = 0;

				if(CamIsZoomingInState != 1 && CamIsZoomingOutState == 1)SetCameraState(CameraData::ZoomOut);
				
				if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
				else if(!CamIsZoomingInState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
				
			}
			break;
		case CameraData::ZoomOut:
			{
				//UE_LOG(LogTemp, Warning, TEXT("ZoomOut"));
				if(CamIsZoomingOutState == 1)CameraBase->ZoomOut(1.f);
				if(CamIsZoomingOutState == 2)CameraBase->ZoomOut(1.f, true);
				if(CamIsZoomingOutState != 1 && CameraBase->CurrentCamSpeed.Z == 0.f) CamIsZoomingOutState = 0;

				if(CamIsZoomingOutState != 1 && CamIsZoomingInState == 1)SetCameraState(CameraData::ZoomIn);
				
				if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
				else if(!CamIsZoomingOutState) CameraBase->SetCameraState(CameraData::UseScreenEdges);
			}
			break;
		case CameraData::ScrollZoomIn:
			{
				//UE_LOG(LogTemp, Warning, TEXT("ScrollZoomIn %f"), ScrollZoomCount);
				if(ScrollZoomCount > 0.f)
				{
					CameraBase->ZoomIn(1.f);
					CameraBase->RotateSpringArm();
					SetCameraZDistance(0);
				}
				if(ScrollZoomCount <= 0.f)
				{
					CameraBase->ZoomIn(1.f, true);
					CameraBase->RotateSpringArm();
					SetCameraZDistance(0);
				}
				
				if(ScrollZoomCount > 0.f)
					ScrollZoomCount -= 0.25f;
				
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
					//CameraBase->SetCameraState(CameraData::LockOnCharacter);
					//CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
				}
			}
			break;
		case CameraData::ScrollZoomOut:
			{
				if(ScrollZoomCount < 0.f)
				{
					CameraBase->ZoomOut(1.f);
					CameraBase->RotateSpringArm(true);
					SetCameraZDistance(0);
				}
				if(ScrollZoomCount >= 0.f)
				{
					CameraBase->ZoomOut(1.f, true);
					CameraBase->RotateSpringArm(true);
					SetCameraZDistance(0);
				}
					
				if(ScrollZoomCount < 0.f)
					ScrollZoomCount += 0.25f;
				
		
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
			break;
		case CameraData::ZoomOutPosition:
			{
				ZoomOutToPosition = true;
				CameraBase->ZoomOutToPosition(CameraBase->ZoomOutPosition);
				CameraBase->RotateSpringArm(true);
				CameraBase->RotateSpringArm(true);
			}
			break;
		case CameraData::ZoomInPosition:
			{
				ZoomOutToPosition = false;
				ZoomInToPosition = true;
				
				if(CameraBase->ZoomInToPosition(CameraBase->ZoomPosition))
				{
					SetCameraZDistance(0);
					ZoomInToPosition = false;
					if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
					else CameraBase->SetCameraState(CameraData::UseScreenEdges);
				}
			}
			break;
		case CameraData::HoldRotateLeft:
			{
				//UE_LOG(LogTemp, Warning, TEXT("HoldRotateLeft"));
				CamIsRotatingRight = false;
				if(LockCameraToCharacter)
				{
					CameraBase->CurrentRotationValue = 0.f;
					CameraBase->SetCameraState(CameraData::LockOnCharacter);
				}
				
				CameraBase->RotateCamLeft(CameraBase->AddCamRotation, !CamIsRotatingLeft); // CameraBase->AddCamRotation
				
				if(!CamIsRotatingLeft && CameraBase->CurrentRotationValue == 0.f)
				{
					CameraBase->CurrentRotationValue = 0.f;
					CameraBase->SetCameraState(CameraData::UseScreenEdges);
				}
				
			}
			break;
		case CameraData::HoldRotateRight:
			{

				//UE_LOG(LogTemp, Warning, TEXT("HoldRotateRight"));
				CamIsRotatingLeft = false;

				if(LockCameraToCharacter)
				{
					CameraBase->CurrentRotationValue = 0.f;
					CameraBase->SetCameraState(CameraData::LockOnCharacter);
				}
				
				CameraBase->RotateCamRight(CameraBase->AddCamRotation, !CamIsRotatingRight); // CameraBase->AddCamRotation
		
				if(!CamIsRotatingRight  && CameraBase->CurrentRotationValue == 0.f)
				{
					CameraBase->CurrentRotationValue = 0.f;
					CameraBase->SetCameraState(CameraData::UseScreenEdges);
				}
				
			}
			break;
		case CameraData::RotateLeft:
			{
				CamIsRotatingRight = false;
				CamIsRotatingLeft = true;
				//UE_LOG(LogTemp, Warning, TEXT("RotateLeft"));

				if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
				
				if(CameraBase->RotateCamLeft(CameraBase->AddCamRotation)) // CameraBase->AddCamRotation
				{
					CamIsRotatingLeft = false;
					if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
				}
				
			}
			break;
		case CameraData::RotateRight:
			{
				CamIsRotatingLeft = false;
				CamIsRotatingRight = true;
				//UE_LOG(LogTemp, Warning, TEXT("RotateRight"));

				if(LockCameraToCharacter) CameraBase->SetCameraState(CameraData::LockOnCharacter);
				if(CameraBase->RotateCamRight(CameraBase->AddCamRotation)) // CameraBase->AddCamRotation
				{
					CamIsRotatingRight = false;
					if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
				}
				
			}
			break;
		case CameraData::LockOnCharacter:
			{
				//UE_LOG(LogTemp, Warning, TEXT("LockOnCharacter"));
				LockCamToCharacter(0);
			}
			break;
		case CameraData::LockOnSpeaking:
			{
				//UE_LOG(LogTemp, Warning, TEXT("LockOnSpeaking"));
				if(SpeakingUnit)
					LockCamToSpecificUnit(SpeakingUnit);
				else
					SetCameraState(CameraData::ZoomToNormalPosition);
			}
			break;
		case CameraData::ZoomToNormalPosition:
			{
				//UE_LOG(LogTemp, Warning, TEXT("ZoomToNormalPosition"));
				
				if(CameraBase->ZoomOutToPosition(CameraBase->ZoomPosition))
				{
					if(CameraBase->RotateCamRight(CameraBase->AddCamRotation))
					{
						CamIsRotatingRight = false;
						CamIsRotatingLeft = false;
						if(!LockCameraToCharacter)CameraBase->SetCameraState(CameraData::UseScreenEdges);
						else CameraBase->SetCameraState(CameraData::LockOnCharacter);
					}
				};
			}
			break;
		case CameraData::ZoomToThirdPerson:
			{
				//UE_LOG(LogTemp, Warning, TEXT("ZoomToThirdPerson"));
				
				if( SelectedUnits.Num())
				{
					FVector SelectedActorLocation = SelectedUnits[0]->GetActorLocation();
				
					CameraBase->LockOnUnit(SelectedUnits[0]);
					if (!CameraBase->IsCameraInAngle())
					{
						CameraBase->RotateCamRight(CameraBase->AddCamRotation);
					}else if(CameraBase->ZoomInToThirdPerson(SelectedActorLocation))
					{
						LockCameraToCharacter = false;
						CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
						CameraBase->SetCameraState(CameraData::ThirdPerson);
					}
		
				}
			}
			break;
		case CameraData::ThirdPerson:
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
					
					//LockZDistanceToCharacter();
					
				}
			}
			break;
		case CameraData::RotateToStart:
				{
					//UE_LOG(LogTemp, Warning, TEXT("RotateToStart"));
					if (FMath::IsNearlyEqual(CameraBase->SpringArmRotator.Yaw, CameraBase->CameraAngles[0], CameraBase->RotationIncreaser))
						CameraBase->SetCameraState(CameraData::MoveToPosition);
				
					CameraBase->RotateCamLeft(CameraBase->AddCamRotation/3);
		
				}
			break;
		case CameraData::MoveToPosition:
			{
				//UE_LOG(LogTemp, Warning, TEXT("MoveCamToPosition"));
				MoveCamToPosition(DeltaTime, CameraBase->OrbitLocation);
			}
			break;
		case CameraData::OrbitAtPosition:
			{
				//UE_LOG(LogTemp, Warning, TEXT("OrbitAtPosition"));
				CamIsRotatingLeft = true;
				CameraBase->OrbitCamLeft(CameraBase->OrbitSpeed);
				
				if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState)
				{
					CamIsRotatingLeft = false;
					CameraBase->SetCameraState(CameraData::MoveWASD);
				}
			}
			break;
		case CameraData::MoveToClick:
			{
				//UE_LOG(LogTemp, Warning, TEXT("MoveToClick"));
				if(ClickedActor)
					MoveCamToClick(DeltaTime, ClickedActor->GetActorLocation());
			}
			break;
		case CameraData::LockOnActor:
			{
				//UE_LOG(LogTemp, Warning, TEXT("LockOnActor"));
				CameraBase->LockOnActor(ClickedActor);
			}
			break;
		case CameraData::OrbitAndMove:
			{
				//UE_LOG(LogTemp, Warning, TEXT("OrbitAndMove"));
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
					// Log message when zooming out
					CameraBase->ZoomOutAutoCam(CameraBase->ZoomPosition+UnitZoomScaler*UnitCountInRange);
				}
				else{
					// Log message when zooming in
					CameraBase->ZoomInToPosition(CameraBase->ZoomPosition);
				}
				
				if(AIsPressedState || DIsPressedState || WIsPressedState || SIsPressedState)
				{
					CamIsRotatingLeft = false;
					CameraBase->SetCameraState(CameraData::MoveWASD);
				}
			}
			break;
		default:
			{
				//UE_LOG(LogTemp, Warning, TEXT("default"));
				CameraBase->SetCameraState(CameraData::UseScreenEdges);
			}
			break;
		}
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

void ACameraControllerBase::MoveCamToPosition(float DeltaSeconds, FVector Destination)
{
    
	const FVector CamLocation = CameraBase->GetActorLocation();
	
	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);
	
	const float Distance = FVector::Distance(CamLocation, Destination); // Use FVector::Distance for distance calculation
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal(); // Use subtraction and GetSafeNormal for direction

	if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
		CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
	else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f; // Adjust this to control movement speed

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

	
	if (Distance <= 50.f) {
		CameraBase->SetCameraState(CameraData::OrbitAtPosition);
	}
}

void ACameraControllerBase::MoveCamToClick(float DeltaSeconds, FVector Destination)
{
    
	const FVector CamLocation = CameraBase->GetActorLocation();
	
	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);
	
	const float Distance = FVector::Distance(CamLocation, Destination); // Use FVector::Distance for distance calculation
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal(); // Use subtraction and GetSafeNormal for direction

	//if (Distance <= 1000.f && CameraBase->CamSpeed+400.f > 200.f)
		//CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 100.f : 0.f;
	//else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 2000.0f? 200.f : 0.f; // Adjust this to control movement speed

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);

	
	if (Distance <= 50.f) {
		CameraBase->SetCameraState(CameraData::LockOnActor);
	}
}

void ACameraControllerBase::MoveCam(float DeltaSeconds, FVector Destination)
{
	
	
	const FVector CamLocation = CameraBase->GetActorLocation();
	
	Destination = FVector(Destination.X, Destination.Y, CamLocation.Z);
	
	const float Distance = FVector::Distance(CamLocation, Destination); // Use FVector::Distance for distance calculation
	if (Distance <= 50.f) return;
	
	const FVector ADirection = (Destination - CamLocation).GetSafeNormal(); // Use subtraction and GetSafeNormal for direction

	if (Distance <= 1000.f && CameraBase->MovePositionCamSpeed > 200.f)
		CameraBase->MovePositionCamSpeed -= CameraBase->MovePositionCamSpeed > 200.0f? 10.f : 0.f;
	else
		CameraBase->MovePositionCamSpeed += CameraBase->MovePositionCamSpeed < 1000.0f? 10.f : 0.f; // Adjust this to control movement speed

	CameraBase->AddActorWorldOffset(ADirection * CameraBase->MovePositionCamSpeed * DeltaSeconds);
}

void ACameraControllerBase::ToggleLockCamToCharacter()
{
	if(IsStrgPressed)
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
		CameraBase->RotateCamLeft(CameraBase->AddCamRotationSpeaking/100);
		
		if(CamIsZoomingInState)
		{
			CameraBase->ZoomIn(1.f);
		}else if(CamIsZoomingOutState)
			CameraBase->ZoomOut(1.f);
		else if(ZoomOutToPosition) CameraBase->ZoomOutToPosition(CameraBase->ZoomOutPosition, SelectedActorLocation);
		else if(ZoomInToPosition && CameraBase->ZoomInToPosition(CameraBase->ZoomPosition,SelectedActorLocation)) ZoomInToPosition = false;
		else if(CameraBase->IsCharacterDistanceTooHigh(Unit->SpeakZoomPosition, SelectedActorLocation))
		{
			CameraBase->ZoomInToPosition(Unit->SpeakZoomPosition, SelectedActorLocation);
			CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - Unit->GetActorLocation().Z);
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
		FVector SelectedActorLocation = SelectedUnits[Index]->GetActorLocation();
		
		CameraBase->LockOnUnit(SelectedUnits[Index]);

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
				if(DeltaYaw > 0)
					CameraBase->RotateCamRight(CameraBase->AddCamRotation*2);
				else
					CameraBase->RotateCamLeft(CameraBase->AddCamRotation*2);
			}
		}

		if(CamIsRotatingRight)
		{
			CamIsRotatingLeft = false;
			CameraBase->RotateCamRight(CameraBase->AddCamRotation); // CameraBase->AddCamRotation
		}
		
		if(CamIsRotatingLeft)
		{
			CamIsRotatingRight = false;
			CameraBase->RotateCamLeft(CameraBase->AddCamRotation); // CameraBase->AddCamRotation
		}
	}else
	{
		LockCameraToCharacter = !LockCameraToCharacter;
		CameraBase->SetCameraState(CameraData::ZoomInPosition);
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

		CameraBase->SetActorLocation(NewPawnLocation);
		CameraBase->CameraDistanceToCharacter = (CameraBase->GetActorLocation().Z - SelectedUnits[0]->GetActorLocation().Z);
	}
}
