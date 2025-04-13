// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/AIController/UnitMovementManager.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/UnitBase.h"                   // Your unit header
#include "Controller/AIController/UnitControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/CrowdAgentInterface.h"

//-------------------------
// Static Instance Getter
//-------------------------
UUnitMovementManager* UUnitMovementManager::GetInstance(UWorld* World)
{
	// For demonstration purposes a simple singleton-like approach is used.
	// In production code consider storing the instance in GameInstance or GameMode.
	static UUnitMovementManager* Instance = nullptr;
	if (!Instance)
	{
		Instance = NewObject<UUnitMovementManager>(World, UUnitMovementManager::StaticClass());
		Instance->AddToRoot();  // Prevent garbage collection
	}
	return Instance;
}

//--------------------------------------
// Command a group of units to move
//--------------------------------------
bool UUnitMovementManager::CommandUnitsToMove(const TArray<AUnitBase*>& Units, const FVector& Destination, AUnitBase* UnitToIgnore)
{
	bool bAllCommandsSuccessful = true;
	for (AUnitBase* Unit : Units)
	{
		// Ensure the unit exists before issuing a command.
		if (Unit)
		{
			bool bSuccess = ExecuteMoveCommand(Unit, Destination, UnitToIgnore);
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to command unit %s to move."), *Unit->GetName());
				bAllCommandsSuccessful = false;
			}
		}
	}
	return bAllCommandsSuccessful;
}

//--------------------------------------
// Execute the move command on one unit
//--------------------------------------
bool UUnitMovementManager::ExecuteMoveCommand(AUnitBase* Unit, const FVector& Destination, AUnitBase* UnitToIgnore)
{
	
	// Retrieve the unit's controller (assumed to be of type AUnitControllerBase).
	AUnitControllerBase* UnitController = Cast<AUnitControllerBase>(Unit->GetController());
	if (!UnitController)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unit %s does not have a valid controller."), *Unit->GetName());
		return false;
	}
	// Validate that the unit and its movement component are valid.
	if (!Unit || !Unit->GetCharacterMovement())
	{
		return false;
	}

	// If the unit is flying, bypass navigation and move directly.
	if (Unit->IsFlying)
	{
		UnitController->DirectMoveToLocation(Unit, Destination);
		return true;
	}

	// Get the current world.
	UWorld* World = Unit->GetWorld();
	if (!World)
	{
		return false;
	}

	// Obtain the navigation system.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Navigation System found for unit %s."), *Unit->GetName());
		return false;
	}

	// Create the move request.
	FAIMoveRequest MoveRequest(Destination);
	MoveRequest.SetAcceptanceRadius(UnitController->AcceptanceRadius); // Assume Unit has an AcceptanceRadius property
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(false);
	// Optionally: configure ignored actors if (UnitToIgnore) { ... }

	// Retrieve the default navigation data.
	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Navigation Data found for unit %s."), *Unit->GetName());
		return false;
	}

	// Prepare the pathfinding query.
	FVector StartLocation = Unit->GetActorLocation();
	FVector EndLocation = Destination;
	FSharedConstNavQueryFilter Filter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());
	FPathFindingQuery Query(*Unit, *NavData, StartLocation, EndLocation, Filter);

	// Execute pathfinding synchronously.
	FPathFindingResult PathResult = NavSys->FindPathSync(Query, EPathFindingMode::Regular);
	TSharedPtr<FNavigationPath, ESPMode::ThreadSafe> NavPath = PathResult.Path; 

	// Check for failure.
	if (!PathResult.IsSuccessful())
	{
		UE_LOG(LogTemp, Warning, TEXT("Pathfinding failed for unit %s. Reason: %s"), *Unit->GetName(), *UEnum::GetValueAsString(PathResult.Result));
		if (PathResult.Result == ENavigationQueryResult::Error)
		{
			Unit->SetUnitState(UnitData::Idle);
		}
		UnitController->DirectMoveToLocation(Unit, Destination);
		return false;
	}

	// If the computed path is partial, issue a fallback command.
	if (NavPath.IsValid() && NavPath->IsPartial())
	{
		UE_LOG(LogTemp, Warning, TEXT("Partial path computed for unit %s. End location: %s"), *Unit->GetName(), *NavPath->GetPathPoints().Last().Location.ToString());
		UnitController->DirectMoveToLocation(Unit, Destination);
		return false;
	}

	if (!NavPath.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid NavPath for unit %s."), *Unit->GetName());
		UnitController->DirectMoveToLocation(Unit, Destination);
		return false;
	}

	// At this point, you can issue the move command.
	// Example: Have the unit follow the computed path.
	// Note: This function (FollowPath) should be implemented in your unit (AUnitBase) or its controller.
	//Unit->FollowPath(NavPath.ToSharedRef());
	// Update the unit's state.
	Unit->SetRunLocation(Destination);
	Unit->UEPathfindingUsed = true;

	return true;
}