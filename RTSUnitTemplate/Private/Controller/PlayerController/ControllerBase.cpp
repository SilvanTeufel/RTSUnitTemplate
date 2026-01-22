// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/PlayerController/ControllerBase.h"
#include "NavigationSystem.h" // Include this for navigation functions
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Core/UnitData.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "NavModifierVolume.h"
#include "Actors/EffectArea.h"
#include "Actors/MissileRain.h"
#include "Actors/UnitSpawnPlatform.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Actors/Waypoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "NavMesh/NavMeshPath.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameModes/ResourceGameMode.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Engine/GameInstance.h"


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
	if (HUDBase && HUDBase->StopLoading && CameraBase) CameraBase->DeSpawnLoadingWidget();
	
	RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	/*
	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(GetPawn());
	if (ExtendedCameraBase)
		ExtendedCameraBase->SetupResourceWidget();
	*/
	
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

float AControllerBase::GetSoundMultiplier() const
{
	float Multiplier = 1.0f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
		{
			Multiplier = Queue->GetGlobalSoundMultiplier() * Queue->GetDefaultSoundVolume();
		}
	}
	return Multiplier;
}

void AControllerBase::DisplayUnitCount()
{
	
	if (RTSGameMode)
	{
		int UnitCount = RTSGameMode->AllUnits.Num();
		UE_LOG(LogTemp, Warning, TEXT("UnitCount: %d"), UnitCount);

		        // Display the message on screen:
                if (GEngine)
                {
                    // The first parameter (-1) creates a new message key.
                    // The second parameter (5.f) is the duration in seconds that the message stays on screen.
                    // The third parameter defines the text color.
                    // The last parameter is the message string, formatted to include the unit count.
                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("UnitCount: %d"), UnitCount));
                }
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
	DOREPLIFETIME(AControllerBase, IsCtrlPressed);
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
	DOREPLIFETIME(AControllerBase, CurrentDraggedAbilityIndicator);
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

	TArray<FPathPoint> PathPoints;

	if(HUDBase && !HUDBase->DisablePathFindingOnEnemy)
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

void AControllerBase::SelectUnit(int Index)
{
	
	if (SelectedUnits[Index] == 0) return;
	
	CurrentUnitWidgetIndex = Index;
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
	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}
	
	SetUnitState_Replication(Unit,1);
	MoveToLocationUEPathFinding(Unit, Location);
}

void AControllerBase::LeftClickAMove_Implementation(AUnitBase* Unit, FVector Location)
{
	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}
	
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
					
				if (Unit && Unit->UnitState != UnitData::Dead)
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


void AControllerBase::FireAbilityMouseHit_Implementation(AUnitBase* Unit, const FHitResult& InHitResult)
{
	if (Unit)
	{
		Unit->FireMouseHitAbility(InHitResult);
	}
}

void AControllerBase::LeftClickSelect_Implementation()
{
	FHitResult Hit_IPoint;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit_IPoint);

	bool Deselect = true;
	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if(SelectedUnits[i])
		{
			if (SelectedUnits[i]->CurrentSnapshot.AbilityClass)
			{
				FireAbilityMouseHit(SelectedUnits[i], Hit_IPoint);
				Deselect = false;
			}else
			{
				SelectedUnits[i]->SetDeselected();
			}
		}
	}

	if (Deselect)
		SelectedUnits.Empty();

}

int AControllerBase::GetHighestPriorityWidgetIndex()
{
	int32 BestIndex = 0;
	int32 MaxAbilities = -1;

	for (int32 i = 0; i < SelectedUnits.Num(); i++)
	{
		if (SelectedUnits[i]->IsWorker) continue;
		// If this is the special CameraUnitWithTag, return immediately
		if (SelectedUnits[i] == CameraUnitWithTag)
		{
			return i;
		}

		// Calculate total number of abilities
		int32 TotalAbilities = 0;
		TotalAbilities += SelectedUnits[i]->DefaultAbilities.Num();
		TotalAbilities += SelectedUnits[i]->SecondAbilities.Num();
		TotalAbilities += SelectedUnits[i]->ThirdAbilities.Num();
		TotalAbilities += SelectedUnits[i]->FourthAbilities.Num();

		// Update BestIndex if this unit has more abilities
		// (or if equal, pick the last one among them)
		if (TotalAbilities >= MaxAbilities)
		{
			BestIndex = i;
			MaxAbilities = TotalAbilities;
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
	
	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}
	
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
	
	if (Unit->CurrentSnapshot.AbilityClass)
	{

		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		if (CancelAbility) CancelCurrentAbility(Unit);
	}
	
	MoveToLocationUEPathFinding(Unit, Location);
	SetUnitState_Replication(Unit,1);
	SetToggleUnitDetection(Unit, false);
}

void AControllerBase::RightClickRunDijkstraPF_Implementation(AUnitBase* Unit, FVector Location, int Counter)
{

	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}
	
	TArray<FPathPoint> PathPoints;

	FVector UnitLocation = Unit->GetActorLocation();
	
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
		// Define the spawn parameters
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		// Use deferred spawning to set TeamId before replication and BeginPlay
		AWaypoint* NewWaypoint = World->SpawnActorDeferred<AWaypoint>(
			WaypointClass,
			FTransform(FRotator::ZeroRotator, NewWPLocation),
			this,
			GetInstigator(),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

		if (NewWaypoint)
		{
			// Assign the team ID from the building
			NewWaypoint->TeamId = BuildingBase->TeamId;
			
			// Finish spawning
			NewWaypoint->FinishSpawning(FTransform(FRotator::ZeroRotator, NewWPLocation));

			// Assign the new waypoint to the building
			BuildingBase->NextWaypoint = NewWaypoint;
			NewWaypoint->AddAssignedUnit(BuildingBase);
			return NewWaypoint;
		}
	}

	return nullptr;
}

void AControllerBase::UnregisterWaypointFromBuilding(ABuildingBase* Building)
{
	if (Building && Building->NextWaypoint)
	{
		AWaypoint* WP = Building->NextWaypoint;
		WP->RemoveAssignedUnit(Building);
		if (HasAuthority() && WP->GetAssignedUnitCount() <= 0)
		{
			WP->Destroy(true, true);
		}
		Building->NextWaypoint = nullptr;
	}
}


void AControllerBase::SetBuildingWaypoint(FVector NewWPLocation, AUnitBase* Unit, AWaypoint*& BuildingWaypoint, bool& PlayWaypointSound, bool& Success)
{
	Success = false;
	ABuildingBase* BuildingBase = Cast<ABuildingBase>(Unit);
	
	PlayWaypointSound = false;
	
	if (!BuildingBase) return;
	if (BuildingBase->CanMove) return;
	if (!BuildingBase->HasWaypoint)
	{
		Success = true;
		return;
	}
	
	PlayWaypointSound = true;
	Success = true;
	
	if (HasAuthority())
	{
		NewWPLocation.Z += RelocateWaypointZOffset;
		
		if (!BuildingWaypoint && BuildingWaypoint != BuildingBase->NextWaypoint)
		{
			if (BuildingBase->NextWaypoint) UnregisterWaypointFromBuilding(BuildingBase);

			BuildingWaypoint = CreateAWaypoint(NewWPLocation, BuildingBase);
		}
		else if (BuildingWaypoint && BuildingWaypoint != BuildingBase->NextWaypoint)
		{
			if (BuildingBase->NextWaypoint) UnregisterWaypointFromBuilding(BuildingBase);

			BuildingBase->NextWaypoint = BuildingWaypoint;
			BuildingWaypoint->AddAssignedUnit(BuildingBase);
				
		}
		else if( BuildingBase->NextWaypoint) BuildingBase->NextWaypoint->SetActorLocation(NewWPLocation);
		else 
		{
			BuildingWaypoint = CreateAWaypoint(NewWPLocation, BuildingBase);
		}
					
		Multi_SetBuildingWaypoint(NewWPLocation, Unit, BuildingWaypoint, PlayWaypointSound);
	}
}

void AControllerBase::Multi_SetBuildingWaypoint_Implementation(FVector NewWPLocation, AUnitBase* Unit, AWaypoint* BuildingWaypoint, bool bPlaySound)
{
	if (!HasAuthority())
	{
		if (ABuildingBase* BuildingBase = Cast<ABuildingBase>(Unit))
		{
			BuildingBase->SetWaypoint(BuildingWaypoint);
			if (BuildingBase->NextWaypoint)
			{
				BuildingBase->NextWaypoint->SetActorLocation(NewWPLocation);
			}
		}
	}
}

void AControllerBase::Server_SetBuildingWaypoint_Implementation(FVector NewWPLocation, AUnitBase* Unit)
{
	AWaypoint* TempWaypoint = nullptr;
	bool TempPlaySound = false;
	bool bSuccess = false;
	SetBuildingWaypoint(NewWPLocation, Unit, TempWaypoint, TempPlaySound, bSuccess);
}

void AControllerBase::Server_Batch_SetBuildingWaypoints_Implementation(const TArray<FVector>& NewWPLocations, const TArray<AUnitBase*>& Units)
{
	AWaypoint* SharedWaypoint = nullptr;
	bool TempPlaySound = false;
	bool bSuccess = false;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		if (Units.IsValidIndex(i) && NewWPLocations.IsValidIndex(i))
		{
			SetBuildingWaypoint(NewWPLocations[i], Units[i], SharedWaypoint, TempPlaySound, bSuccess);
		}
	}
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

void AControllerBase::DrawCircleAtLocation(UWorld* World, const FVector& Location, FColor CircleColor)
{
	if (HUDBase)
	{
		HUDBase->AddClickIndicator(Location, CircleColor);
	}
}

#define COLLISION_NAVMODIFIER ECC_GameTraceChannel1
FVector AControllerBase::TraceRunLocation(FVector RunLocation, bool& HitNavModifier)
{
    // Setup trace start and end positions. A 2000-unit trace (1000 up, 1000 down) is robust.
    FVector TraceStart = RunLocation + FVector(0, 0, 1000.0f);
    FVector TraceEnd = RunLocation - FVector(0, 0, 1000.0f);

    // --- SETUP FOR OBJECT QUERY ---
    // This is the core change. We specify WHICH KINDS of objects we want to hit.
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);          // For Landscape, static meshes, etc.
   // ObjectQueryParams.AddObjectTypesToQuery(COLLISION_NAVMODIFIER);   // For our custom NavModifier volumes.
    // ---

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = true; // Use complex collision for landscapes

    // --- ACTOR IGNORING LOGIC (Unchanged from your original code) ---
    // Add all AWorkArea actors to the ignore list.
    for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
    {
       QueryParams.AddIgnoredActor(*It);
    }

    // Add all AUnitBase actors to the ignore list.
    for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
    {
       QueryParams.AddIgnoredActor(*It);
    }
    // ---

    FHitResult HitResult;

    // Perform the trace using LineTraceSingleByObject with our combined object query.
    if (GetWorld()->LineTraceSingleByObjectType(HitResult, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
    {
        // The trace hit something. Set the location to the impact point.
        RunLocation = HitResult.ImpactPoint;

        // Now, we must identify WHAT we hit.
        ANavModifierVolume* NavModifierVolume = Cast<ANavModifierVolume>(HitResult.GetActor());
        if (NavModifierVolume)
        {
           // SUCCESS: We hit our specific volume.
           //UE_LOG(LogTemp, Warning, TEXT("TraceRunLocation successfully hit a NavModifierVolume: %s"), *NavModifierVolume->GetName());
           HitNavModifier = true;
        }
        else
        {
           // The cast failed, so it must be a WorldStatic object (Landscape, etc.).
           //UE_LOG(LogTemp, Log, TEXT("TraceRunLocation hit a WorldStatic object: %s"), *HitResult.GetActor()->GetName());
           HitNavModifier = false;
        }

        // Optional: Draw a debug line to see the successful trace
        //DrawDebugLine(GetWorld(), TraceStart, HitResult.ImpactPoint, FColor::Green, false, 4.0f, 0, 1.0f);
    }
    else
    {
        // The trace hit nothing (e.g., traced into an empty sky or pit).
        // The original RunLocation is returned, and HitNavModifier is false.
        HitNavModifier = false;
        //UE_LOG(LogTemp, Warning, TEXT("TraceRunLocation failed to hit any valid object."));

        // Optional: Draw a debug line to see the failed trace
        //DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Red, false, 4.0f, 0, 1.0f);
    }

    return RunLocation;
}

/*
FVector AControllerBase::TraceRunLocation(FVector RunLocation, bool& HitNavModifier)
{
	// Setup trace start and end positions (adjust as needed).
	FVector TraceStart = RunLocation + FVector(0, 0, 1000);
	FVector TraceEnd = RunLocation - FVector(0, 0, 1000);

	FCollisionQueryParams QueryParams;
	
	// Add all AWorkArea actors to the ignore list.
	for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}

	// Add all AUnitBase actors to the ignore list.
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}

	FHitResult HitResult;
	QueryParams.bTraceComplex = true;
	
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		// Set NewWPLocation to the impact point from the trace.
		RunLocation = HitResult.ImpactPoint;

		ANavModifierVolume* NavModifierVolume = Cast<ANavModifierVolume>(HitResult.GetActor());
		if (NavModifierVolume)
		{
			UE_LOG(LogTemp, Warning, TEXT("Successfully hit a NavModifierVolume"));
			HitNavModifier = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Hit actor is not a NavModifierVolume"));
			HitNavModifier = false;
		}
	}

	return RunLocation;
}
*/

void AControllerBase::RunUnitsAndSetWaypoints(FHitResult Hit)
{
	int32 NumUnits = SelectedUnits.Num();
	//int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)NumUnits));
	const int32 GridSize = ComputeGridSize(NumUnits);
	AWaypoint* BWaypoint = nullptr;

	bool PlayWaypointSound = false;
	bool PlayRunSound = false;
	
	TArray<AUnitBase*> BuildingUnits;
	TArray<FVector>    BuildingLocs;

	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		if (SelectedUnits[i] != CameraUnitWithTag)
		if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead) {
			
			//FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
			int32 Row = i / GridSize;     // Row index
			int32 Col = i % GridSize;     // Column index

			//FVector RunLocation = Hit.Location + FVector(Col * 100, Row * 100, 0.f);  // Adjust x and y positions equally for a square grid
			FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);

			bool HitNavModifier;
			RunLocation = TraceRunLocation(RunLocation, HitNavModifier);
			if (HitNavModifier) continue;
			
			ABuildingBase* BuildingBase = Cast<ABuildingBase>(SelectedUnits[i]);
			if (BuildingBase && !BuildingBase->CanMove)
			{
				BuildingUnits.Add(SelectedUnits[i]);
				BuildingLocs.Add(RunLocation);
				if (BuildingBase->HasWaypoint) PlayWaypointSound = true;
			}
			else if (IsShiftPressed) {
				DrawCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				RightClickRunShift(SelectedUnits[i], RunLocation); // _Implementation
				PlayRunSound = true;
			}else if(UseUnrealEnginePathFinding)
			{
				DrawCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				RightClickRunUEPF(SelectedUnits[i], RunLocation, true); // _Implementation
				PlayRunSound = true;
			}
			else {
				DrawCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				RightClickRunDijkstraPF(SelectedUnits[i], RunLocation, i); // _Implementation
				PlayRunSound = true;
			}
		}
	}

	if (BuildingUnits.Num() > 0)
	{
		Server_Batch_SetBuildingWaypoints(BuildingLocs, BuildingUnits);
	}

	if (WaypointSound && PlayWaypointSound)
	{
		UGameplayStatics::PlaySound2D(this, WaypointSound, GetSoundMultiplier());
	}

	if (RunSound && PlayRunSound)
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastRunSoundTime >= RunSoundDelayTime) // Check if 3 seconds have passed
		{
			UGameplayStatics::PlaySound2D(this, RunSound, GetSoundMultiplier());
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
	/*
	AUnitBase* NewSelection = nullptr;
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		// Every Controller can Check his own TeamId
		if (Unit && Unit->GetUnitState() != UnitData::Dead && Unit->UnitIndex == UnitIndex)
		{
			NewSelection = Unit;
			break;
		}
	}
	
	if (NewSelection && NewSelection->UnitState != UnitData::Dead)
	{
		if(NewSelection)
			NewSelection->SetToggleUnitDetection(true);
	}*/


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
			/*
			if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead)
			{
				if(SelectedUnits[i])
					SelectedUnits[i]->SetToggleUnitDetection(true);
			}*/
			ToggleUnitDetection(SelectedUnits[i]);
		}
	}
}

void AControllerBase::AReleased()
{

}

void AControllerBase::JumpCamera()
{
	if (CameraBase && IsSpacePressed) //  && (IsCtrlPressed || IsSpacePressed)
	{
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		CameraBase->JumpCamera(Hit);
	}
}


void AControllerBase::StrgPressed() {
	IsCtrlPressed = true;
}

void AControllerBase::StrgReleased() {
	IsCtrlPressed = false;
}

void AControllerBase::ZoomIn()
{
	if (CameraBase && IsCtrlPressed)
	{
		CameraBase->ZoomIn(1.f);
	}
}

void AControllerBase::ZoomOut()
{
	if (CameraBase && IsCtrlPressed)
	{
		CameraBase->ZoomOut(1.f);
	}
}


void AControllerBase::SpawnMissileRain(int TeamId, FVector Location) // FVector TargetLocation
{
	if (!MissileRainClass) return;

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
	if (!EAClass) return;
	
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
	UnitBase->DespawnCurrentAbilityIndicator();
}

void AControllerBase::Multi_SetControllerTeamId_Implementation(int Id)
{
	SelectableTeamId = Id;
	OnRep_SelectableTeamId();
}

void AControllerBase::OnRep_SelectableTeamId()
{
	for (TActorIterator<AWaypoint> It(GetWorld()); It; ++It)
	{
		if (*It)
		{
			It->UpdateVisibility();
		}
	}
}

void AControllerBase::Multi_SetControllerDefaultWaypoint_Implementation(AWaypoint* Waypoint)
{
	if(Waypoint)
		DefaultWaypoint = Waypoint;
}

void AControllerBase::AddUnitToChase_Implementation(AUnitBase* DetectingUnit, AActor* OtherActor)
{
	if (DetectingUnit)
	{
		DetectingUnit->AddUnitToChase(OtherActor);
	}
}


void AControllerBase::RemoveUnitToChase_Implementation(AUnitBase* DetectingUnit, AActor* OtherActor)
{

	// Check if the actor is valid, not pending kill, and its low-level memory is okay.
	if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
	{
		return;
	}
	    
	AUnitBase* Unit = Cast<AUnitBase>(OtherActor);
	
	if (!Unit) return;
	if (!DetectingUnit) return;

	    
	// Remove the unit from the chasing list.
	if (DetectingUnit->UnitsToChase.Contains(Unit))
		DetectingUnit->UnitsToChase.RemoveSingle(Unit);

}