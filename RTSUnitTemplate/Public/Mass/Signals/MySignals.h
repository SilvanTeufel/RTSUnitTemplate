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
	//const FName ReachedDestination = FName("UnitReachedDestination");
	const FName Idle(TEXT("Idle"));
	const FName Chase(TEXT("Chase"));
	const FName Attack(TEXT("Attack"));
	const FName Dead(TEXT("Dead"));
	const FName PatrolIdle(TEXT("PatrolIdle"));
	const FName PatrolRandom(TEXT("PatrolRandom"));
	const FName Pause(TEXT("Pause"));
	const FName Run(TEXT("Run"));
	const FName Casting(TEXT("Cast"));
	// Define other signal names here if needed
	const FName MeleeAttack(TEXT("MeeleAttack"));
	const FName RangedAttack(TEXT("RangedAttack"));
	const FName UnitInDetectionRange(TEXT("UnitInDetectionRange"));
}

USTRUCT()
struct FUnitPresenceSignal
{
	GENERATED_BODY()

	// Using UPROPERTY can help with debugging/reflection but adds minor overhead.
	// Remove UPROPERTY() if not needed.
	
	UPROPERTY()
	FMassEntityHandle SignalerEntity; // The entity broadcasting this signal

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector; // Use NetQuantize for potential network relevance

	UPROPERTY()
	int32 TeamId = -1;

	UPROPERTY()
	bool bIsInvisible = false;

	UPROPERTY()
	bool bIsFlying = false;

	// Add other relevant data that detectors might need without accessing fragments directly
	// e.g., maybe a faction identifier, or specific target type flags
};
// Add other custom signal structs here if needed