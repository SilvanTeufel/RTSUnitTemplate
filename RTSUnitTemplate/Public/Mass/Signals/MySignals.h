// In UnitSignals.h or a similar header
#pragma once

#include "CoreMinimal.h" // Or Engine minimal header
#include "MySignals.generated.h"

/** Signal raised when a unit controlled by UnitMovementProcessor reaches its destination */
USTRUCT(BlueprintType) // Add BlueprintType if you might want to use it in BP/StateTree UI
struct FUnitReachedDestinationSignal // No FMassSignal inheritance needed
{
	GENERATED_BODY()

	// You can still add payload data if needed
	//UPROPERTY()
	//FVector Location;
};

namespace UnitSignals
{
	const FName ReachedDestination = FName("UnitReachedDestination");
	// Define other signal names here if needed
}
// Add other custom signal structs here if needed