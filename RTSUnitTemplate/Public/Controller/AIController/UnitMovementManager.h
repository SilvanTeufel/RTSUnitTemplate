
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

#include "UnitMovementManager.generated.h"

// Forward declarations to avoid cyclic dependencies
class AUnitBase;
class ANavigationData;

/**
 * UUnitMovementManager
 * 
 * A centralized manager for commanding one or more units to move to a location.  
 * This class encapsulates the shared pathfinding logic and can be used to reduce redundancy 
 * when multiple units share the same destination.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API UUnitMovementManager : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Returns (or creates) an instance of the movement manager.
	 * In a real implementation, you may want to store this as part of your GameInstance.
	 */
	static UUnitMovementManager* GetInstance(UWorld* World);

	/**
	 * Commands a list of units to move to a given destination.
	 *
	 * @param Units			The units to command.
	 * @param Destination	The target location.
	 * @param UnitToIgnore	(Optional) A unit that should be ignored during pathfinding.
	 * @return				True if the command was successfully issued for all units.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unit Movement")
	bool CommandUnitsToMove(const TArray<AUnitBase*>& Units, const FVector& Destination, AUnitBase* UnitToIgnore = nullptr);

protected:
	/**
	 * Executes the move command for an individual unit.
	 *
	 * @param Unit			The unit to command.
	 * @param Destination	The target location.
	 * @param UnitToIgnore	(Optional) A unit to ignore in the pathfinding query.
	 * @return				True if the command was successfully issued.
	 */
	bool ExecuteMoveCommand(AUnitBase* Unit, const FVector& Destination, AUnitBase* UnitToIgnore);
};
