// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Unit/TransportUnit.h"

#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"

void ATransportUnit::BindTransportOverlap()
{
	if (IsATransporter)
	{
		UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
		if (CapsuleComp)
		{
			CapsuleComp->OnComponentBeginOverlap.AddDynamic(this, &ATransportUnit::OnCapsuleOverlapBegin);
		}
	}
}

void ATransportUnit::KillLoadedUnits()
{
	if (IsATransporter)
	{
		for (ATransportUnit* LoadedUnit : LoadedUnits)
		{
			if (LoadedUnit)
			{
				SetUnitState(UnitData::Dead);
			}
		}
	}
}


void ATransportUnit::SetRdyForTransport(bool Rdy)
{
	RdyForTransport = Rdy;
}

void ATransportUnit::SetCollisionAndVisibility_Implementation(bool IsVisible)
{
	StopVisibilityTick = !IsVisible;
	IsInitialized = IsVisible;
	SetActorEnableCollision(IsVisible);
	
	
	if (!IsVisible)
	{
		RdyForTransport = IsVisible;
		SetCharacterVisibility(IsVisible);
	}
}

void ATransportUnit::LoadUnit(AUnitBase* UnitToLoad)
{
	if (!IsATransporter || UnitToLoad == this) return;

	if (UnitToLoad->UnitSpaceNeeded > MaxSpacePerUnitAllowed) return;

	if (TransportId != 0 && TransportId != UnitToLoad->TransportId)
	{
		UnitToLoad->SetRdyForTransport(false);
		return;
	}
	if (UnitToLoad && (CurrentUnitsLoaded + UnitToLoad->UnitSpaceNeeded) <= MaxTransportUnits)
	{
		// Instead of disabling avoidance entirely, adjust the avoidance group.
		if (UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(UnitToLoad->GetMovementComponent()))
		{
			MovementComponent->SetMovementMode(EMovementMode::MOVE_Flying);
		}
		UnitToLoad->SetCollisionAndVisibility(false);
		UnitToLoad->SetActorLocation(VoidLocation);
		// Disable collisions for the entire actor.
		// Add the unit to the loaded units array.
		CurrentUnitsLoaded = CurrentUnitsLoaded + UnitToLoad->UnitSpaceNeeded;
		LoadedUnits.Add(UnitToLoad);

		GetWorld()->GetTimerManager().ClearTimer(UnloadTimerHandle);
		LoadedUnit();
	}
}

void ATransportUnit::UnloadAllUnits()
{
	if (!IsATransporter) return;
    
	// Start the gradual unload process.
	UnloadNextUnit();
}

void ATransportUnit::UnloadNextUnit()
{
	
	// Check if there are still units to unload.
	if (LoadedUnits.Num() > 0)
	{
		// Get the first unit from the array.
		ATransportUnit* LoadedUnit = LoadedUnits[0];

		CurrentUnitsLoaded = CurrentUnitsLoaded - LoadedUnit->UnitSpaceNeeded;
		
		if (LoadedUnit)
		{
			// Use the transporter's location as the base.
			FVector BaseLocation  = GetActorLocation();
			
			// Generate random offsets in the range [-200, -100] or [100, 200].
			float XOffset = FMath::RandRange(UnloadVariationMin, UnloadVariatioMax) * (FMath::RandBool() ? 1 : -1);
			float YOffset = FMath::RandRange(UnloadVariationMin, UnloadVariatioMax) * (FMath::RandBool() ? 1 : -1);
            
			FVector UnloadLocation = BaseLocation  + FVector(XOffset, YOffset, 0.f);

			// Perform a line trace to find the ground.
			FVector TraceStart = UnloadLocation + FVector(0.f, 0.f, 1000.f);
			FVector TraceEnd = UnloadLocation - FVector(0.f, 0.f, 2000.f);
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.AddIgnoredActor(LoadedUnit);

			if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
			{
				// If we hit the ground, adjust the unload location.
				float CapsuleHalfHeight = 0.f;
				if (UCapsuleComponent* CapsuleComp = LoadedUnit->FindComponentByClass<UCapsuleComponent>())
				{
					CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
				}
				// Set the Z to the ground hit location plus capsule height and a Z offset.
				UnloadLocation.Z = HitResult.Location.Z + CapsuleHalfHeight + 10.f;
			}
			else
			{
				// Fallback in case no ground is hit.
				UnloadLocation.Z += 100.f;
			}
			
			// Apply any additional unload offset.
			LoadedUnit->SetActorLocation(UnloadLocation + UnloadOffset);

			// Re-enable collision.
			//LoadedUnit->SetActorEnableCollision(true);
			LoadedUnit->SetUnitState(UnitData::Idle);
			// Reset the movement mode back to walking.
			if (UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(LoadedUnit->GetMovementComponent()))
			{
				MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
			}

			// Re-enable visibility and mark as initialized.
			LoadedUnit->SetCollisionAndVisibility(true);
		}

		// Remove the unloaded unit from the array.
		LoadedUnits.RemoveAt(0);

		UnloadedUnit();
		// Schedule the next unload after a 1-second delay.
		GetWorld()->GetTimerManager().SetTimer(UnloadTimerHandle, this, &ATransportUnit::UnloadNextUnit, UnloadInterval, false);
	}
	else
	{
		// No more units to unload; clear the timer.
		GetWorld()->GetTimerManager().ClearTimer(UnloadTimerHandle);
	}
}

void ATransportUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATransportUnit, CanBeTransported);
	DOREPLIFETIME(ATransportUnit, MaxTransportUnits);
	DOREPLIFETIME(ATransportUnit, LoadedUnits);
	DOREPLIFETIME(ATransportUnit, IsInitialized);
	DOREPLIFETIME(ATransportUnit, InstantLoadRange);
	DOREPLIFETIME(ATransportUnit, RdyForTransport);
	DOREPLIFETIME(ATransportUnit, IsATransporter);
	DOREPLIFETIME(ATransportUnit, VoidLocation);
	DOREPLIFETIME(ATransportUnit, UnloadOffset);
	DOREPLIFETIME(ATransportUnit, UnloadVariationMin);
	DOREPLIFETIME(ATransportUnit, UnloadVariatioMax);
	DOREPLIFETIME(ATransportUnit, CurrentUnitsLoaded);

	DOREPLIFETIME(ATransportUnit, UnitSpaceNeeded);
	DOREPLIFETIME(ATransportUnit, MaxSpacePerUnitAllowed);

	DOREPLIFETIME(ATransportUnit, UnloadInterval);
	DOREPLIFETIME(ATransportUnit, TransportId);
}

void ATransportUnit::OnCapsuleOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{

	// Ensure this unit is a transporter.
	if (!IsATransporter)
	{
		return;
	}
	
	// Make sure the other actor is valid and not self.
	if (OtherActor && OtherActor != this)
	{
		// Attempt to cast the overlapping actor to ATransportUnit.
		if (AUnitBase* OverlappingUnit = Cast<AUnitBase>(OtherActor))
		{
			// Check if the overlapping unit is ready for transport.
			if (OverlappingUnit->RdyForTransport)
			{
				// Load the overlapping unit.
				LoadUnit(OverlappingUnit);
			}
		}
	}
}