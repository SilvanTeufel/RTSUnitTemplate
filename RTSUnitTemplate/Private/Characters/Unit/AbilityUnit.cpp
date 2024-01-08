// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/AbilityUnit.h"

#include "Characters/Unit/SpeakingUnit.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound\SoundCue.h"

void AAbilityUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityUnit, UnitState);
	DOREPLIFETIME(AAbilityUnit, UnitStatePlaceholder);
}

void AAbilityUnit::TeleportToValidLocation(const FVector& Destination)
{
	FVector Start = Destination + FVector(0.f, 0.f, 1000.f);
	FVector End = Destination - FVector(0.f, 0.f, 200.f);
	FHitResult HitResult;

	// Perform a line trace to check if the location is valid
	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility))
	{
		// If the hit location is valid (e.g., on the ground), teleport to that location
		if (HitResult.bBlockingHit)
		{
			// Optionally, you might want to add additional checks on HitResult to ensure it's a valid surface
			SetActorLocation(FVector(HitResult.Location.X, HitResult.Location.Y, HitResult.Location.Z + 70.f));
			return;
		}
	}

	SetActorLocation(GetActorLocation());
}

void AAbilityUnit::StartAcceleratingTowardsDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart)
{
	if (GetUnitState() == UnitData::Dead && FVector::Dist(GetActorLocation(), ChargeDestination) >= RequiredDistanceToStart && FVector::Dist(GetActorLocation(), ChargeDestination) <= Attributes->Range.GetCurrentValue())
		return;
	
	ChargeDestination = NewDestination;
	TargetVelocity = NewTargetVelocity;
	AccelerationRate = NewAccelerationRate;
	RequiredDistanceToStart = NewRequiredDistanceToStart;
	CurrentVelocity = FVector::ZeroVector;
	
	// Start a repeating timer which calls the Accelerate function
	GetWorld()->GetTimerManager().SetTimer(AccelerationTimerHandle, this, &AAbilityUnit::Accelerate, 0.1f, true);
}

void AAbilityUnit::Accelerate()
{
	if (GetUnitState() == UnitData::Dead)
		return;
	// If the actor is close enough to the destination, stop accelerating
	if(FVector::Dist(GetActorLocation(), ChargeDestination) <= Attributes->Range.GetCurrentValue()) //Attributes->Range.GetCurrentValue()+10.f
	{
		// Stop the character if it's past the destination
		//LaunchCharacter(FVector::ZeroVector, false, false);
		GetWorld()->GetTimerManager().ClearTimer(AccelerationTimerHandle);
	}else if (FVector::Dist(GetActorLocation(), ChargeDestination) <= RequiredDistanceToStart)
	{
		// Gradually increase CurrentVelocity towards TargetVelocity
		CurrentVelocity = FMath::VInterpTo(CurrentVelocity, TargetVelocity, GetWorld()->DeltaTimeSeconds, AccelerationRate);

		// Launch character with the current velocity
		LaunchCharacter(CurrentVelocity, true, true);
	}
	

	// If the current velocity is approximately equal to the target velocity, clear the timer
	if (CurrentVelocity.Equals(TargetVelocity, 1.0f))
	{
		GetWorld()->GetTimerManager().ClearTimer(AccelerationTimerHandle);
	}
}

void AAbilityUnit::SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState)
{
	UnitState = NewUnitState;
}

TEnumAsByte<UnitData::EState> AAbilityUnit::GetUnitState()
{
	return UnitState;
}