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

	return true;
}