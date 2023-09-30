// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/ControllerBase.h"
#include "NavigationSystem.h" // Include this for navigation functions
#include "Controller/CameraControllerBase.h"
#include "AIController.h"
#include "Actors/EffectArea.h"
#include "Actors/MissileRain.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

AControllerBase::AControllerBase() {
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}


void AControllerBase::BeginPlay() {
	
		CameraBase = Cast<ACameraBase>(GetPawn());
		HUDBase = Cast<APathProviderHUD>(GetHUD());
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

	if(LeftClickIsPressed)
	{
		FHitResult Hit_CPoint;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit_CPoint);
		HUDBase->CPoint = Hit_CPoint.Location;
	}
	
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	
	if(AttackToggled)
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if(SelectedUnits[i] && SelectedUnits[i]->GetUnitState() == UnitData::Idle && SelectedUnits[i]->ToggleUnitDetection)
		HUDBase->ControllDirectionToMouse(SelectedUnits[i], Hit);
	}

	TArray<FPathPoint> PathPoints;
	if(!HUDBase->DisablePathFindingOnEnemy)
	for (int32 i = 0; i < HUDBase->EnemyUnitBases.Num(); i++)
		if( HUDBase->EnemyUnitBases[i] && !HUDBase->EnemyUnitBases[i]->TeamId &&  HUDBase->EnemyUnitBases[i]->DijkstraSetPath)
		{
			SetRunLocationUseDijkstraForAI(HUDBase->EnemyUnitBases[i]->DijkstraEndPoint, HUDBase->EnemyUnitBases[i]->DijkstraStartPoint, HUDBase->EnemyUnitBases, PathPoints, i);
			HUDBase->EnemyUnitBases[i]->DijkstraSetPath = false;
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

void AControllerBase::LeftClickPressed()
{
	LeftClickIsPressed = true;
	
	if (AttackToggled) {
		AttackToggled = false;
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead) {
				FHitResult Hit;
				GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
				
				FHitResult Hit_Pawn;
				GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);

				if (Hit_Pawn.bBlockingHit)
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
					
					if(UnitBase && !UnitBase->TeamId)
					{
						/// Focus Enemy Units ///
						SelectedUnits[i]->UnitToChase = UnitBase;
						SelectedUnits[i]->SetUnitState(UnitData::Chase);
					}else if(UseUnrealEnginePathFinding)
					{
					
						if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead)
						{
							/// A-Move Units ///
							FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
							DrawDebugSphere(GetWorld(), RunLocation, 15, 5, FColor::Green, false, 1.5, 0, 1);
							MoveToLocationUEPathFinding(SelectedUnits[i], RunLocation);
							SelectedUnits[i]->SetUnitState(UnitData::Run);
						}
					
					}else
					{
						/// A-Move Units ///
						FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0);
						DrawDebugSphere(GetWorld(), RunLocation, 15, 5, FColor::Green, false, 1.5, 0, 1);
						
						SelectedUnits[i]->SetUnitState(UnitData::Run);
						SelectedUnits[i]->RunLocationArray.Empty();
						SelectedUnits[i]->RunLocationArrayIterator = 0;
						SelectedUnits[i]->RunLocation = RunLocation;
						//SelectedUnits[i]->UnitStatePlaceholder = UnitData::Run;
					}
				}
				
			}
			
		}
		
	}
	else {

		FHitResult Hit_IPoint;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit_IPoint);
		HUDBase->IPoint = Hit_IPoint.Location;
		
		HUDBase->DeselectAllUnits();

		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			if(SelectedUnits[i])
			SelectedUnits[i]->SetDeselected();
		}
		
		SelectedUnits.Empty();
		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);
		
		if (Hit_Pawn.bBlockingHit)
		{
			AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
			const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Hit_Pawn.GetActor());
			
			if (UnitBase && UnitBase->TeamId && !SUnit)
			{
				HUDBase->SetUnitSelected(UnitBase);

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
}

void AControllerBase::MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation)
{
	if (!Unit || !Unit->GetCharacterMovement() || !Unit->GetController())
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

	//Unit->SetUnitState(UnitData::RunUEPathfinding);
	Unit->RunLocation = DestinationLocation;
	Unit->UEPathfindingUsed = true;
	// Move the unit to the destination location using the navigation system
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(DestinationLocation);
	MoveRequest.SetAcceptanceRadius(5.0f); // Set an acceptance radius for reaching the destination

	FNavPathSharedPtr NavPath;
	AIController->MoveTo(MoveRequest, &NavPath);
}

void AControllerBase::RightClickPressed()
{
	AttackToggled = false;
	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead) {
			if (IsShiftPressed) {
			
						FHitResult Hit;
						GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
						FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0);
						DrawDebugSphere(GetWorld(), RunLocation, 15, 5, FColor::Green, false, 1.5, 0, 1);
						if(!SelectedUnits[i]->RunLocationArray.Num())
						{
							SelectedUnits[i]->RunLocation = RunLocation;
							SelectedUnits[i]->UEPathfindingUsed = false;
							SelectedUnits[i]->SetUnitState(UnitData::Run);
						}
						
						SelectedUnits[i]->RunLocationArray.Add(RunLocation);
						//SelectedUnits[i]->UnitStatePlaceholder = UnitData::Idle;
						SelectedUnits[i]->UnitsToChase.Empty();
						SelectedUnits[i]->UnitToChase = nullptr;
						SelectedUnits[i]->ToggleUnitDetection = false;
					
			}else if(UseUnrealEnginePathFinding && !SelectedUnits[i]->IsFlying)
			{	
						FHitResult Hit;
						GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
						FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
						DrawDebugSphere(GetWorld(), RunLocation, 15, 5, FColor::Green, false, 1.5, 0, 1);
						MoveToLocationUEPathFinding(SelectedUnits[i], RunLocation);
						SelectedUnits[i]->ToggleUnitDetection = false;
						SelectedUnits[i]->SetUnitState(UnitData::Run);
				
			}
			else {
				TArray<FPathPoint> PathPoints;
			
						FVector UnitLocation = SelectedUnits[i]->GetActorLocation();
						
						FHitResult Hit;
						GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
						FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);

						DrawDebugCircle(GetWorld(), FVector(RunLocation.X, RunLocation.Y, RunLocation.Z+2.f), 15, 5, FColor::Green, false, 1.5, 0, 1, FVector(0, 1, 0), FVector(1, 0, 0));
						if(SelectedUnits[i]->GetUnitState() != UnitData::Run)
						SelectedUnits[i]->SetWalkSpeed(0.f);

						SelectedUnits[i]->UEPathfindingUsed = false;
						SelectedUnits[i]->SetUnitState(UnitData::Run);
						SelectedUnits[i]->RunLocationArray.Empty();
						SelectedUnits[i]->RunLocationArrayIterator = 0;
						SelectedUnits[i]->RunLocation = RunLocation;
						//SelectedUnits[i]->UnitStatePlaceholder = UnitData::Idle;
						SelectedUnits[i]->ToggleUnitDetection = false;

						float Range = FVector::Dist(UnitLocation, Hit.Location);

						if(!HUDBase->DisablePathFindingOnFriendly && Range >= HUDBase->RangeThreshold && !HUDBase->IsLocationInNoPathFindingAreas(Hit.Location))
							SetRunLocationUseDijkstra(Hit.Location, UnitLocation, SelectedUnits, PathPoints, i);
				
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
			Units[i]->RunLocation = PathPoints[0].Point + FVector(i / 2 * 50, i % 2 * 50, 0.f);;
					
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
			Units[i]->RunLocation = PathPoints[0].Point;;
					
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


void AControllerBase::TPressed()
{
	if(!AttackToggled)
	{
		AttackToggled = true;
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead)
			{
				if(SelectedUnits[i])
				SelectedUnits[i]->ToggleUnitDetection = true;
			}
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

void AControllerBase::SpawnEffectArea(int TeamId, FVector Location)
{

	FTransform Transform;
	Transform.SetLocation(Location);
	Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator

		
	const auto MyEffectArea = Cast<AEffectArea>
						(UGameplayStatics::BeginDeferredActorSpawnFromClass
						(this, EffectAreaClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	
	if (MyEffectArea != nullptr)
	{
		MyEffectArea->TeamId = TeamId;
	
		MyEffectArea->Mesh->OnComponentBeginOverlap.AddDynamic(MyEffectArea, &AEffectArea::OnOverlapBegin);
		MyEffectArea->Mesh->OnComponentEndOverlap.AddDynamic(MyEffectArea, &AEffectArea::OnOverlapEnd);

		UGameplayStatics::FinishSpawningActor(MyEffectArea, Transform);
	}
	
}