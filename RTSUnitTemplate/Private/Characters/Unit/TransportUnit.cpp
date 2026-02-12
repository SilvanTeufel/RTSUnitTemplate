// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Unit/TransportUnit.h"

#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Widgets/UnitTimerWidget.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Mass/UnitMassTag.h"

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
	// KillLoadedUnitsIfDestroyed
	if (IsATransporter && KillLoadedUnitsIfDestroyed)
	{
		for (ATransportUnit* LoadedUnit : LoadedUnits)
		{
			if (LoadedUnit)
			{
				LoadedUnit->SetHealth(0);
				
			}
		}
	}else if (IsATransporter)
	{
		for (ATransportUnit* LoadedUnit : LoadedUnits)
		{
			if (LoadedUnit)
			{
				UnloadNextUnit();
			}
		}
	}
}


void ATransportUnit::SetRdyForTransport(bool Rdy)
{
	RdyForTransport = Rdy;
}

void ATransportUnit::SetCollisionAndVisibility(bool IsVisible)
{
	StopVisibilityTick = !IsVisible;
	IsInitialized = IsVisible;
	//SetActorEnableCollision(IsVisible);
	
	
	//if (!IsVisible)
	{
		RdyForTransport = IsVisible;
		SetCharacterVisibility(IsVisible);
	}
}

void ATransportUnit::LoadUnit(AUnitBase* UnitToLoad)
{
	if (!IsATransporter || UnitToLoad == this || !UnitToLoad) return;
	if (UnitToLoad->TeamId != TeamId) return;

	if (LoadedUnits.Contains(UnitToLoad)) return;

	if (UnitToLoad->UnitSpaceNeeded > MaxSpacePerUnitAllowed) return;

	// If the unit has been assigned a specific transporter,
	// only allow loading into that exact transporter.
	if (UnitToLoad && (TransportId != 0 && TransportId != UnitToLoad->TransportId))
	{
		return;
	}
	
	if (UnitToLoad && (CurrentUnitsLoaded + UnitToLoad->UnitSpaceNeeded) <= MaxTransportUnits)
	{
		// Apply replicated load effects so all clients update the unit consistently
		MulticastApplyLoadEffects(UnitToLoad, GetMassActorLocation());
		
		CurrentUnitsLoaded = CurrentUnitsLoaded + UnitToLoad->UnitSpaceNeeded;
		LoadedUnits.Add(UnitToLoad);

		GetWorld()->GetTimerManager().ClearTimer(UnloadTimerHandle);
		LoadedUnit(LoadedUnits.Num());
		
		if (UUnitTimerWidget* TimerWidget = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject())) // Assuming you have a UUnitBaseTimer class for the timer widget
		{
			TimerWidget->MyWidgetIsVisible = true;
			TimerWidget->TimerTick();
		}
	}
}

void ATransportUnit::UnloadAllUnits()
{
	if (!IsATransporter) return;
    
	// Start the gradual unload process.
	UnloadNextUnit();

	if (UUnitTimerWidget* TimerWidget = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject())) // Assuming you have a UUnitBaseTimer class for the timer widget
	{
		TimerWidget->MyWidgetIsVisible = false;
	}
}

void ATransportUnit::UnloadNextUnit()
{
	if (LoadedUnits.Num() > 0)
	{
		AUnitBase* LoadedUnit = Cast<AUnitBase>(LoadedUnits[0]);
		
		if (LoadedUnit)
		{
			CurrentUnitsLoaded = CurrentUnitsLoaded - LoadedUnit->UnitSpaceNeeded;
			FVector BaseLocation  = GetMassActorLocation();
			
			float XOffset = FMath::RandRange(UnloadVariationMin, UnloadVariatioMax) * (FMath::RandBool() ? 1 : -1);
			float YOffset = FMath::RandRange(UnloadVariationMin, UnloadVariatioMax) * (FMath::RandBool() ? 1 : -1);
            
			FVector UnloadLocation = BaseLocation  + FVector(XOffset, YOffset, 0.f);
			
			FVector TraceStart = UnloadLocation + FVector(0.f, 0.f, 1000.f);
			FVector TraceEnd = UnloadLocation - FVector(0.f, 0.f, 2000.f);
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.AddIgnoredActor(LoadedUnit);

			if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
			{
				float CapsuleHalfHeight = 0.f;
				if (UCapsuleComponent* CapsuleComp = LoadedUnit->FindComponentByClass<UCapsuleComponent>())
				{
					CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
				}
				UnloadLocation.Z = HitResult.Location.Z + CapsuleHalfHeight + 10.f;
			}
			else
			{
				UnloadLocation.Z += 100.f;
			}
			const FVector FinalUnloadLocation = UnloadLocation + UnloadOffset;
			
			
			// Apply replicated unload effects so all clients update the unit consistently
			MulticastApplyUnloadEffects(LoadedUnit, FinalUnloadLocation);
			/*
			if (UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(LoadedUnit->GetMovementComponent()))
			{
				MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
			}
			*/
			LoadedUnit->UpdateEntityStateOnUnload(FinalUnloadLocation);
		}
		
		LoadedUnits.RemoveAt(0);
		UnloadedUnit(LoadedUnits.Num());
		
		GetWorld()->GetTimerManager().SetTimer(UnloadTimerHandle, this, &ATransportUnit::UnloadNextUnit, UnloadInterval, false);
	}
	else
	{
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
	DOREPLIFETIME(ATransportUnit, CanBeSelected);
	DOREPLIFETIME(ATransportUnit, InstantLoadRange);
	DOREPLIFETIME(ATransportUnit, RdyForTransport);
	DOREPLIFETIME(ATransportUnit, IsATransporter);
	//DOREPLIFETIME(ATransportUnit, VoidLocation);
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
			// Check if the overlapping unit is ready for transport and matches this transporter's ID.
			if (OverlappingUnit->RdyForTransport && OverlappingUnit->TransportId != 0 && OverlappingUnit->TransportId == TransportId)
			{
				// Load the overlapping unit.
				LoadUnit(OverlappingUnit);
			}
		}
	}
}

void ATransportUnit::MulticastApplyUnloadEffects_Implementation(AUnitBase* LoadedUnit, const FVector& FinalUnloadLocation)
{
	if (!LoadedUnit) return;

	LoadedUnit->SetActorLocation(FinalUnloadLocation);
	LoadedUnit->SetTranslationLocation(FinalUnloadLocation);
	LoadedUnit->UpdatePredictionFragment(FinalUnloadLocation, LoadedUnit->Attributes->GetBaseRunSpeed());
	LoadedUnit->StopMassMovement();
	LoadedUnit->EnableDynamicObstacle(true);
	LoadedUnit->EditUnitDetectable(true);
	LoadedUnit->SetCollisionAndVisibility(true);

	LoadedUnit->SwitchEntityTagByState(UnitData::Idle, LoadedUnit->UnitStatePlaceholder);
	LoadedUnit->CanAttack = true;
	LoadedUnit->IsInitialized = true;
	LoadedUnit->CanActivateAbilities = true;
	LoadedUnit->CanBeSelected = true;
}

void ATransportUnit::MulticastApplyLoadEffects_Implementation(AUnitBase* UnitToLoad, const FVector& TransporterLocation)
{
	if (!UnitToLoad) return;

	UnitToLoad->CanAttack = false;
	UnitToLoad->IsInitialized = false;
	UnitToLoad->CanActivateAbilities = false;
	UnitToLoad->CanBeSelected = false;
	UnitToLoad->AddStopMovementTagToEntity();
	UnitToLoad->SetCollisionAndVisibility(false);
	UnitToLoad->SetActorLocation(TransporterLocation);
	UnitToLoad->EnableDynamicObstacle(false);
	UnitToLoad->EditUnitDetectable(false);
}
