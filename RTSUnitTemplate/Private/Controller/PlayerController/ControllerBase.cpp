// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/PlayerController/ControllerBase.h"
#include "NavigationSystem.h" // Include this for navigation functions
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Core/UnitData.h"
#include "AIController.h"
#include "Landscape.h"
#include "Actors/EffectArea.h"
#include "Actors/MissileRain.h"
#include "Actors/UnitSpawnPlatform.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "NavMesh/NavMeshPath.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameModes/ResourceGameMode.h"
//#include "Engine/Engine.h"
//#include "Engine/EngineTypes.h"    // For FOverlapResult and related collision types
//#include "Engine/OverlapResult.h"


AControllerBase::AControllerBase() {
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}


void AControllerBase::BeginPlay() {

	Super::BeginPlay();
	
	InitCameraHUDGameMode();
	ToggleUnitCountDisplay(ShowUnitCount);
	LastRunSoundTime = 0.f;
}
void AControllerBase::InitCameraHUDGameMode()
{
	CameraBase = Cast<ACameraBase>(GetPawn());
	HUDBase = Cast<APathProviderHUD>(GetHUD());
	if(HUDBase && HUDBase->StopLoading && CameraBase) CameraBase->DeSpawnLoadingWidget();
	
	RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if (!RTSGameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("!!!!!DONT Found GameMode Inside ControllerBase!!!!"));
	}
}

void AControllerBase::ToggleUnitCountDisplay(bool bEnable)
{
	if (bEnable)
	{
		// Start the timer to call DisplayFPS every 2 seconds
		GetWorldTimerManager().SetTimer(FPSTimerHandle, this, &AControllerBase::DisplayUnitCount, 2.0f, true);
	}
	else
	{
		// Stop the timer if it's running
		GetWorldTimerManager().ClearTimer(FPSTimerHandle);
	}
}

void AControllerBase::DisplayUnitCount()
{
	
	if (RTSGameMode)
	{
		int UnitCount = RTSGameMode->AllUnits.Num();
		UE_LOG(LogTemp, Warning, TEXT("UnitCount: %d"), UnitCount);
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
	DOREPLIFETIME(AControllerBase, UEPathfindingCornerOffset); // Added for Build
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
	
	if(AttackToggled)
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if(SelectedUnits[i] &&
			!SelectedUnits[i]->IsA(ABuildingBase::StaticClass()) &&
			SelectedUnits[i]->GetUnitState() == UnitData::Idle &&
			SelectedUnits[i]->ToggleUnitDetection &&
			SelectedUnits[i]->ControlUnitIntoMouseDirection)
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

	/*
	if (RTSGameMode)
	{
		for (AActor* Actor : RTSGameMode->AllUnits)
		{
			AUnitBase* Unit = Cast<AUnitBase>(Actor);
			Unit->VisibilityTick();
		}
	}
	*/
	
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
	
	if (SelectedUnits[Index] == 0) return;
	
	CurrentUnitWidgetIndex = 0;
	HUDBase->DeselectAllUnits();
	HUDBase->SetUnitSelected( SelectedUnits[Index]);
	SelectedUnits = HUDBase->SelectedUnits; 

	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);

	if(ExtendedCameraBase && SelectedUnits[0])
	{
		ExtendedCameraBase->UpdateSelectorWidget();
		
	}
}

void AControllerBase::LeftClickAMoveUEPF_Implementation(AUnitBase* Unit, FVector Location)
{
	//DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Red, false, 1.5, 0, 1);
	if (Unit->ActivatedAbilityInstance && !Unit->ActivatedAbilityInstance->AbilityCanBeCanceled) return;
	
	SetUnitState_Replication(Unit,1);
	MoveToLocationUEPathFinding(Unit, Location);
}

void AControllerBase::LeftClickAMove_Implementation(AUnitBase* Unit, FVector Location)
{
	if (Unit->ActivatedAbilityInstance && !Unit->ActivatedAbilityInstance->AbilityCanBeCanceled) return;
	
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

int AControllerBase::GetHighestPriorityWidgetIndex()
{
	// Index of the highest-priority unit found so far
	int32 BestIndex = 0;  
	// Store the best priority found so far; start with -1 so any non-empty set is better
	int32 BestPriority = -1;    

	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		// If this is the special CameraUnitWithTag, return immediately
		if (SelectedUnits[i] == CameraUnitWithTag)
		{
			return i;
		}

		// Compute a priority score based on how many ability arrays are non-empty
		int32 Priority = 0;
		Priority += (SelectedUnits[i]->DefaultAbilities.Num() > 0) ? 1 : 0;
		Priority += (SelectedUnits[i]->SecondAbilities.Num()  > 0) ? 1 : 0;
		Priority += (SelectedUnits[i]->ThirdAbilities.Num()   > 0) ? 1 : 0;
		Priority += (SelectedUnits[i]->FourthAbilities.Num()  > 0) ? 1 : 0;

		// Update BestIndex if this unit has an equal or greater priority
		// (so we get the last unit among equals)
		if (Priority >= BestPriority)
		{
			BestIndex = i;
			BestPriority = Priority;
		}
	}

	return BestIndex;
}

void AControllerBase::SetWidgets(int Index)
{
	if (!HUDBase->SelectedUnits.Num()) return;
	
	SelectedUnits = HUDBase->SelectedUnits;

	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
	
	if (SelectedUnits.IsValidIndex(Index) && SelectedUnits.Num() && SelectedUnits[Index] && ExtendedCameraBase) {
	
		AUnitBase* UnitBase = Cast<AUnitBase>(SelectedUnits[Index]);
		
		if(UnitBase)
		{
			CurrentUnitWidgetIndex = Index;
			ExtendedCameraBase->SetUserWidget(UnitBase);
			ExtendedCameraBase->SetSelectorWidget(Index, UnitBase);
			return;
		}
	}
	
	ExtendedCameraBase->SetUserWidget(nullptr);
	
}

void AControllerBase::SetRunLocation_Implementation(AUnitBase* Unit, const FVector& DestinationLocation)
{
	Unit->SetRunLocation(DestinationLocation);
}

/*
void AControllerBase::MoveToLocationUEPathFinding_Implementation(AUnitBase* Unit, const FVector& DestinationLocation)
{
    if (!HasAuthority() || !Unit)
    {
        return;
    }

    UCharacterMovementComponent* MovementComp = Unit->GetCharacterMovement();
    if (!MovementComp)
    {
        return;
    }

    AAIController* AIController = Cast<AAIController>(Unit->GetController());
    if (!AIController)
    {
        return;
    }

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSystem)
    {
        return;
    }

    // Attempt to perform line trace as a workaround
    FHitResult HitResult;
    if (PerformLineTrace(Unit, DestinationLocation, HitResult))
    {
        // If line trace hits a pawn, handle the obstacle
        OnLineTraceHit(Unit, DestinationLocation);
    }
    else
    {
        // No obstacle detected; proceed with movement
        ContinueMoveToLocation(Unit, DestinationLocation);
    }
}

bool AControllerBase::PerformLineTrace(AUnitBase* Unit, const FVector& DestinationLocation, FHitResult& HitResult)
{
    FVector StartLocation = Unit->GetActorLocation();
    FVector EndLocation = DestinationLocation;

	EndLocation.Z = StartLocation.Z;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Unit); // Ignore the unit itself

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECC_Pawn, // Trace against pawns only
        QueryParams
    );

	// Draw the line trace for visualization
	FColor TraceColor = bHit ? FColor::Red : FColor::Green;
	DrawDebugLine(GetWorld(), StartLocation, EndLocation, TraceColor, false, 2.0f, 0, 1.0f);

	
    return bHit;
}

void AControllerBase::OnLineTraceHit(AUnitBase* Unit, const FVector& DestinationLocation)
{
	// Log or handle the hit as needed
	UE_LOG(LogTemp, Warning, TEXT("LineTrace hit a pawn. Scheduling movement retry."));
    
	// Calculate the alternate location
	FVector AlternateLocation = CalculateAlternateLocation(Unit, DestinationLocation);
	ContinueMoveToLocation(Unit, AlternateLocation);
	// Set a timer to retry movement after 3 seconds

	FTimerDelegate TimerDel = FTimerDelegate::CreateUObject(this, &AControllerBase::ContinueMoveToLocation, Unit, DestinationLocation);
    
	GetWorld()->GetTimerManager().SetTimer(
		MoveRetryTimerHandle,
		TimerDel,
		1.0f,
		false
	);
}

FVector AControllerBase::CalculateAlternateLocation(AUnitBase* Unit, const FVector& DestinationLocation)
{
	FVector CurrentLocation = Unit->GetActorLocation();
	FVector Direction = DestinationLocation - CurrentLocation;
    
	// Zero out the Z component to ensure movement in the XY plane
	Direction.Z = 0.0f;
    

	if (Direction.IsNearlyZero())
	{
		return DestinationLocation; // Fallback to original destination
	}
    
	Direction = Direction.GetSafeNormal();
    
	// Calculate the right vector perpendicular to the direction in the XY plane
	FVector RightVector = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
    
	if (RightVector.IsNearlyZero())
	{
		return DestinationLocation; // Fallback to original destination
	}
    
	// Calculate a new location offset by 90 degrees to the right
	FVector AlternateLocation = CurrentLocation + (RightVector * 500.f);

	DrawDebugSphere(GetWorld(), AlternateLocation, 50.0f, 12, FColor::Blue, false, 5.0f);

	return AlternateLocation;
}

void AControllerBase::ContinueMoveToLocation(AUnitBase* Unit, FVector DestinationLocation)
{
    if (!HasAuthority() || !Unit)
    {
        return;
    }

	UCharacterMovementComponent* MovementComp = Unit->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	AAIController* AIController = Cast<AAIController>(Unit->GetController());
	if (!AIController)
	{
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("ContinueMoveToLocation DestinationLocation: %s"), *DestinationLocation.ToString());

	DrawDebugSphere(GetWorld(), DestinationLocation, 50.0f, 12, FColor::Silver, false, 5.0f);
    // Set the run location and mark that pathfinding has been used
    SetRunLocation(Unit, DestinationLocation);
    Unit->UEPathfindingUsed = true;
	SetUnitState_Replication(Unit, 1);
    // Prepare the move request
    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(DestinationLocation);
    MoveRequest.SetAcceptanceRadius(5.0f);
    //MoveRequest.SetUsePathfinding(true);
    //MoveRequest.SetCanStrafe(false);
   //MoveRequest.SetAllowPartialPath(false);

    FNavPathSharedPtr NavPath;

    // Execute the move
    EPathFollowingRequestResult::Type MoveResult = AIController->MoveTo(MoveRequest, &NavPath);

    if (MoveResult == EPathFollowingRequestResult::RequestSuccessful && NavPath)
    {
    	UE_LOG(LogTemp, Warning, TEXT("!Successful movement!"));
        FNavMeshPath* NavMeshPath = NavPath->CastPath<FNavMeshPath>();
        if (NavMeshPath)
        {
            NavMeshPath->OffsetFromCorners(UEPathfindingCornerOffset);
        }
    }
}
*/

void AControllerBase::MoveToLocationUEPathFinding_Implementation(AUnitBase* Unit, const FVector& DestinationLocation)
{

	if(!HasAuthority())
	{
		return;
	}
	
	if (!Unit)
	{
		return;
	}


	// Enable avoidance on the movement component
	UCharacterMovementComponent* MovementComp = Unit->GetCharacterMovement();

	if (!MovementComp)
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

	// Workaround because avoidance is not working.
	// Do a LineTrace to the Destination. If the Linetrace hits a Pawn then:
	// Start a Timer which executes the rest of the code NavPath + MoveRequest in 3 Seconds.
	// Calc a new Calculation 90° to the Location of the Destionation
	// and Execute the Rest of the Code with the Calculated Location
	
		
	SetRunLocation(Unit, DestinationLocation);
	Unit->UEPathfindingUsed = true;
	Unit->SetUEPathfinding = true;
	// Move the unit to the destination location using the navigation system
	/*
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(DestinationLocation);
	MoveRequest.SetAcceptanceRadius(5.0f); // Set an acceptance radius for reaching the destination
	MoveRequest.SetUsePathfinding(true); // Ensure pathfinding is used
	MoveRequest.SetCanStrafe(false); // Example setting, adjust as needed
	MoveRequest.SetAllowPartialPath(false); // Example setting, adjust as needed


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
	*/
}

void AControllerBase::SetUnitState_Multi_Implementation(AUnitBase* Unit, int State)
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
	//Unit->UnitsToChase.Empty();
	//Unit->UnitToChase = nullptr;
}
void AControllerBase::RightClickRunShift_Implementation(AUnitBase* Unit, FVector Location)
{
	if (Unit->ActivatedAbilityInstance && !Unit->ActivatedAbilityInstance->AbilityCanBeCanceled) return;
	
	DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Green, false, 1.5, 0, 1);
	if(!Unit->RunLocationArray.Num())
	{
		SetRunLocation(Unit, Location);
		Unit->UEPathfindingUsed = false;

		SetUnitState_Replication(Unit,1);
	}
						
	Unit->RunLocationArray.Add(Location);
	//Unit->UnitsToChase.Empty();
	//Unit->UnitToChase = nullptr;
	Unit->SetToggleUnitDetection(false);
}

void AControllerBase::RightClickRunUEPF_Implementation(AUnitBase* Unit, FVector Location, bool CancelAbility)
{
	
	if (!Unit) return;

	if (Unit->ActivatedAbilityInstance && !Unit->ActivatedAbilityInstance->AbilityCanBeCanceled) return;
	
	if (CancelAbility) CancelCurrentAbility(Unit);
	
	DrawDebugSphere(GetWorld(), Location, 15, 5, FColor::Green, false, 1.5, 0, 1);
	MoveToLocationUEPathFinding(Unit, Location);
	SetUnitState_Replication(Unit,1);
	SetToggleUnitDetection(Unit, false);
}

void AControllerBase::RightClickRunDijkstraPF_Implementation(AUnitBase* Unit, FVector Location, int Counter)
{
	if (Unit->ActivatedAbilityInstance && !Unit->ActivatedAbilityInstance->AbilityCanBeCanceled) return;
	
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

AWaypoint* AControllerBase::CreateAWaypoint(FVector NewWPLocation, ABuildingBase* BuildingBase)
{
	UWorld* World = GetWorld();

	if (World && WaypointClass)
	{
		// Define the spawn parameters (can customize this further if needed)
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		// Spawn the waypoint actor
		AWaypoint* NewWaypoint = World->SpawnActor<AWaypoint>(WaypointClass, NewWPLocation, FRotator::ZeroRotator, SpawnParams);

		if (NewWaypoint)
		{
			// Assign the new waypoint to the building
			BuildingBase->NextWaypoint = NewWaypoint;
			BuildingBase->NextWaypoint->TeamId = BuildingBase->TeamId;
			return BuildingBase->NextWaypoint;
		}
	}

	return nullptr;
}


bool AControllerBase::SetBuildingWaypoint(FVector NewWPLocation, AUnitBase* Unit, AWaypoint*& BuildingWaypoint, bool& PlayWaypointSound)
{

			ABuildingBase* BuildingBase = Cast<ABuildingBase>(Unit);
	PlayWaypointSound = false;
	
	if (!BuildingBase) return false;
	if (!BuildingBase->HasWaypoint) return true;

	PlayWaypointSound = true;
	
	NewWPLocation.Z += RelocateWaypointZOffset;
	if (!BuildingWaypoint && BuildingWaypoint != BuildingBase->NextWaypoint)
	{
		if (BuildingBase->NextWaypoint) BuildingBase->NextWaypoint->Destroy(true, true);

		BuildingWaypoint = CreateAWaypoint(NewWPLocation, BuildingBase);
	}
	else if (BuildingWaypoint && BuildingWaypoint != BuildingBase->NextWaypoint)
	{
		if (BuildingBase->NextWaypoint) BuildingBase->NextWaypoint->Destroy(true, true);

		BuildingBase->NextWaypoint = BuildingWaypoint;
			
	}
	else if( BuildingBase->NextWaypoint) BuildingBase->NextWaypoint->SetActorLocation(NewWPLocation);
	else 
	{
		BuildingWaypoint = CreateAWaypoint(NewWPLocation, BuildingBase);
	}
				
	return true;
}



int32 AControllerBase::ComputeGridSize(int32 NumUnits) const
{
	switch (GridFormationShape)
	{
	case EGridShape::VerticalLine:
		return 1; // Single column
	case EGridShape::Square:
	case EGridShape::Staggered:
	default:
		return FMath::CeilToInt(FMath::Sqrt(static_cast<float>(NumUnits)));
	}
}

FVector AControllerBase::CalculateGridOffset(int32 Row, int32 Col) const
{
	switch (GridFormationShape)
	{
	case EGridShape::Staggered:
		{
			const float XOffset = (Row % 2 == 0) 
				? Col * GridSpacing
				: Col * GridSpacing + GridSpacing * 0.5f;
			return FVector(XOffset, Row * GridSpacing, 0.f);
		}
            
	case EGridShape::VerticalLine:
		return FVector(0.f, Row * GridSpacing, 0.f);
            
	case EGridShape::Square:
	default:
		return FVector(Col * GridSpacing, Row * GridSpacing, 0.f);
	}
}

void AControllerBase::RunUnitsAndSetWaypoints(FHitResult Hit)
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
			const FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);
			
			if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
			{
				//PlayWaypointSound = true;
			}else if (IsShiftPressed) {
				RightClickRunShift_Implementation(SelectedUnits[i], RunLocation);
				PlayRunSound = true;
			}else if(UseUnrealEnginePathFinding && !SelectedUnits[i]->IsFlying)
			{
				RightClickRunUEPF_Implementation(SelectedUnits[i], RunLocation, true);
				PlayRunSound = true;
			}
			else {
				RightClickRunDijkstraPF_Implementation(SelectedUnits[i], RunLocation, i);
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

void AControllerBase::DeQueAbility_Implementation(AUnitBase* UnitBase, int ButtonIndex)
{
	UnitBase->DequeueAbility(ButtonIndex);
}

void AControllerBase::CancelCurrentAbility_Implementation(AUnitBase* UnitBase)
{
	UnitBase->SetUnitState(UnitData::Idle);
	UnitBase->UnitControlTimer = 0;
	UnitBase->CancelCurrentAbility();
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

