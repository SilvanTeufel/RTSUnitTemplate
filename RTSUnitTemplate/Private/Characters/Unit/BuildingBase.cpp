// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/BuildingBase.h"
#include "Actors/EnergyWall.h"
#include "Net/UnrealNetwork.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameModes/ResourceGameMode.h"
#include "GameModes/RTSGameModeBase.h"
#include "Components/CapsuleComponent.h"
#include "Math/RotationMatrix.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "EngineUtils.h"
#include "System/RTSBeaconSubsystem.h"
#include "Actors/WorkArea.h"
#include "Actors/Waypoint.h"
#include "Characters/Unit/WorkingUnitBase.h"


ABuildingBase::ABuildingBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Buildings are stationary and may use ISMs for visuals; no dedicated SnapMesh is needed.
	bIsBuilding = true;
	CanMove = false;
	bUseSkeletalMovement = false;
}

void ABuildingBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABuildingBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABuildingBase, EnergyWallArray);
	DOREPLIFETIME(ABuildingBase, bHasRallyPoint);
	DOREPLIFETIME(ABuildingBase, RallyPointLocation);
}

void ABuildingBase::SetRallyPoint(FVector NewLocation, AWorkArea* ResourceArea)
{
	if (!HasAuthority())
	{
		return;
	}

	RallyPointLocation = NewLocation;
	bHasRallyPoint = true;
	RallyResourceArea = ResourceArea;
}

FVector ABuildingBase::GetRallyPointLocation() const
{
	// The waypoint the player drags with a right-click is the existing, visible rally (the HUD even
	// draws the line to it), so it wins over anything computed here.
	if (HasWaypoint && IsValid(NextWaypoint))
	{
		return NextWaypoint->GetActorLocation();
	}

	if (bHasRallyPoint)
	{
		return RallyPointLocation;
	}

	// No point set: put one on the edge of the base, pointing away from the rest of our buildings.
	// Units otherwise pile up on the spot they spawned at - which is inside the base - and block both
	// the next unit out of this building and the workers walking through.
	const FVector Here = GetActorLocation();
	FVector Centroid = FVector::ZeroVector;
	int32 Count = 0;

	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<ABuildingBase> It(World); It; ++It)
		{
			const ABuildingBase* Other = *It;
			if (!IsValid(Other) || Other == this || Other->TeamId != TeamId)
			{
				continue;
			}
			Centroid += Other->GetActorLocation();
			++Count;
		}
	}

	FVector Outward = GetActorForwardVector();
	if (Count > 0)
	{
		Centroid /= Count;
		const FVector Away = Here - Centroid;
		if (!Away.IsNearlyZero())
		{
			Outward = Away;
		}
	}

	Outward.Z = 0.f;
	Outward = Outward.GetSafeNormal();
	if (Outward.IsNearlyZero())
	{
		Outward = FVector::ForwardVector;
	}

	FVector Target = Here + Outward * DefaultRallyDistance;

	// Drop it onto the ground, or the unit walks at a point floating over a slope.
	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		if (World->LineTraceSingleByChannel(Hit, Target + FVector(0.f, 0.f, 2000.f), Target - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Params))
		{
			Target.Z = Hit.Location.Z;
		}
	}

	return Target;
}

void ABuildingBase::ApplyRallyPointToUnit(AUnitBase* NewUnit)
{
	if (!HasAuthority() || !IsValid(NewUnit) || NewUnit->GetUnitState() == UnitData::Dead)
	{
		return;
	}

	// A worker rallied onto a deposit goes straight to work. Mirrors SendWorkerToResource, including
	// its permission and capacity checks - skipping those would have the worker walk over and bounce.
	if (NewUnit->IsWorker && IsValid(RallyResourceArea))
	{
		if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(NewUnit))
		{
			if (Worker->CanMineWorkArea(RallyResourceArea) && AWorkArea::HasFreeMiningSlotFor(RallyResourceArea, Worker))
			{
				Worker->SetResourcePlace(RallyResourceArea, /*bRegisterOnNewPlace=*/true);
				Worker->AutoMining = true;
				NewUnit->SetUEPathfinding = true;   // declared on AUnitBase, not on AWorkingUnitBase
				NewUnit->SetUnitState(UnitData::GoToResourceExtraction);
				NewUnit->SwitchEntityTagByState(UnitData::GoToResourceExtraction, NewUnit->UnitStatePlaceholder);
				return;
			}
		}
	}

	// Everything else walks to the point. Workers without a rallied deposit are left alone: their own
	// GoToBase/SwitchResourceArea cycle already assigns them, and a move order would only cancel it.
	if (NewUnit->IsWorker)
	{
		return;
	}

	NewUnit->RunLocation = GetRallyPointLocation();
	NewUnit->SetUEPathfinding = true;
	NewUnit->SetUnitState(UnitData::Run);
	NewUnit->SwitchEntityTagByState(UnitData::Run, NewUnit->UnitStatePlaceholder);

}

void ABuildingBase::ApplySupplyCapacity()
{
	if (bSupplyCapacityApplied || !HasAuthority())
	{
		return;
	}

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
	if (!ResourceGameMode)
	{
		return;
	}

	// Respect the same ceiling the Reactor uses, so a stack of supply buildings cannot run away with it.
	if (ResourceGameMode->GetMaxResource(EResourceType::Rare, TeamId) >= SupplyCapacityLimit)
	{
		return;
	}

	ResourceGameMode->IncreaseMaxResources(SupplyCapacityGain, TeamId);
	bSupplyCapacityApplied = true;
}

void ABuildingBase::ReleaseSupplyCapacity()
{
	// Hand the capacity back, otherwise losing supply buildings would still leave the team able to train.
	if (!bSupplyCapacityApplied || !HasAuthority())
	{
		return;
	}

	if (AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		ResourceGameMode->DecreaseMaxResources(SupplyCapacityGain, TeamId);
	}
	bSupplyCapacityApplied = false;
}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();

	// Gebaeude sperren Einheiten nicht mehr physisch - das Umlaufen regelt das Navigationsnetz.
	// Anklicken laeuft ueber ECC_Visibility und bleibt davon unberuehrt.
	if (UCapsuleComponent* Kapsel = GetCapsuleComponent())
	{
		Kapsel->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}


	if (EnergyWallClass && Origin)
	{
		SpawnEnergyWall(EnergyWallClass, Origin);
	}

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(ResourceGameMode)
		ResourceGameMode->AddBaseToGroup(this);

	// Grant this building's supply capacity. Deferred a moment because TeamId is assigned after spawn - the
	// Singularian Reactor's Blueprint waits half a second for exactly the same reason.
	const bool bGrantsCapacity =
		SupplyCapacityGain.PrimaryCost || SupplyCapacityGain.SecondaryCost || SupplyCapacityGain.TertiaryCost ||
		SupplyCapacityGain.RareCost || SupplyCapacityGain.EpicCost || SupplyCapacityGain.LegendaryCost;

	if (HasAuthority() && bGrantsCapacity)
	{
		FTimerHandle SupplyHandle;
		GetWorldTimerManager().SetTimer(SupplyHandle, this, &ABuildingBase::ApplySupplyCapacity, 0.5f, false);
	}

	// Same deferral reason as the supply grant above: TeamId is only set after spawn.
	if (HasAuthority() && bAutoLoadNearbyWorkers && IsATransporter)
	{
		FTimerHandle AutoLoadHandle;
		GetWorldTimerManager().SetTimer(AutoLoadHandle, this, &ABuildingBase::AutoLoadNearbyWorkers,
		                                FMath::Max(0.1f, AutoLoadDelaySeconds), false);
	}
}

bool ABuildingBase::IsOwnedByAiTeam() const
{
	// Twin of AWorkArea::IsOwnedByAiTeam - kept local rather than shared, because the only common base
	// of the two classes is AActor and neither header can reasonably pull in the other.
	//
	// A team can carry MORE than one controller. In the AI test level team 2 has both a human camera
	// controller (bIsAi=false) and an RLAgent that actually plays it (bIsAi=true). Returning the first
	// match therefore answered "player" for an AI team - measured, not guessed. The team counts as AI
	// if ANY of its controllers is one; a genuine human team simply has no AI controller.
	const UWorld* World = GetWorld();
	if (!World) return false;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const AControllerBase* PC = Cast<AControllerBase>(It->Get()))
		{
			if (PC->SelectableTeamId == TeamId && PC->bIsAi)
			{
				return true;
			}
		}
	}
	return false;
}

void ABuildingBase::AutoLoadNearbyWorkers()
{
	if (!HasAuthority() || !IsATransporter) return;
	if (CurrentUnitsLoaded >= MaxTransportUnits) return;

	// "No AI controller for this team" means "not yet" as much as it means "human" during the opening
	// seconds - see AutoLoadOwnerResolveSeconds. Keep asking rather than guessing wrong once.
	const UWorld* Welt = GetWorld();
	const float Jetzt = Welt ? Welt->GetTimeSeconds() : 0.f;
	if (!IsOwnedByAiTeam() && Jetzt < AutoLoadOwnerResolveSeconds)
	{
		FTimerHandle ErneutHandle;
		GetWorldTimerManager().SetTimer(ErneutHandle, this, &ABuildingBase::AutoLoadNearbyWorkers, 1.f, false);
		return;
	}

	// The human player gets exactly the worker they sent. Grabbing the four nearest workers off their
	// resource nodes is what the AI needs (it cannot click them in reliably), but for a player it is an
	// unasked-for order that silently stalls their income the moment a reactor finishes.
	if (!IsOwnedByAiTeam())
	{
		// Alle Arbeiter der Baustelle, nicht nur den, der den letzten Schlag gesetzt hat.
		TArray<AUnitBase*> Bauleute;
		for (const TWeakObjectPtr<AUnitBase>& Schwach : BuilderWorkers)
		{
			if (AUnitBase* W = Schwach.Get())
			{
				Bauleute.AddUnique(W);
			}
		}
		if (AUnitBase* Letzter = BuilderWorker.Get())
		{
			Bauleute.AddUnique(Letzter);
		}

		int32 Geladen = 0;
		for (AUnitBase* Worker : Bauleute)
		{
			if (CurrentUnitsLoaded >= MaxTransportUnits) break;
			if (!IsValid(Worker) || !Worker->IsWorker || !Worker->CanBeTransported) continue;
			if (Worker->TeamId != TeamId || Worker->GetUnitState() == UnitData::Dead) continue;

			const int32 Vorher = CurrentUnitsLoaded;
			LoadUnit(Worker);
			if (CurrentUnitsLoaded > Vorher) ++Geladen;
		}

		UE_LOG(LogTemp, Warning, TEXT("[AutoLoad] %s (team %d, Spieler): Bauleute=%d geladen=%d now=%d/%d"),
		       *GetName(), TeamId, Bauleute.Num(), Geladen, CurrentUnitsLoaded, MaxTransportUnits);
		return;
	}

	// Nearest first, so the building takes the crew standing next to it rather than pulling workers
	// across the map away from their resource nodes.
	TArray<AUnitBase*> Candidates;
	const FVector Here = GetActorLocation();
	const float RadiusSq = AutoLoadWorkerRadius * AutoLoadWorkerRadius;

	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* Worker = *It;
		if (!IsValid(Worker) || Worker == this) continue;
		if (!Worker->IsWorker || !Worker->CanBeTransported) continue;
		if (Worker->TeamId != TeamId) continue;
		if (Worker->GetUnitState() == UnitData::Dead) continue;
		if (FVector::DistSquared(Here, Worker->GetMassActorLocation()) > RadiusSq) continue;
		Candidates.Add(Worker);
	}

	Candidates.Sort([Here](const AUnitBase& A, const AUnitBase& B)
	{
		return FVector::DistSquared(Here, A.GetActorLocation()) < FVector::DistSquared(Here, B.GetActorLocation());
	});

	int32 Loaded = 0;
	for (AUnitBase* Worker : Candidates)
	{
		if (CurrentUnitsLoaded >= MaxTransportUnits) break;
		const int32 Before = CurrentUnitsLoaded;
		LoadUnit(Worker);
		if (CurrentUnitsLoaded > Before) ++Loaded;
	}

	UE_LOG(LogTemp, Warning, TEXT("[AutoLoad] %s (team %d, KI): candidates=%d loaded=%d now=%d/%d"),
	       *GetName(), TeamId, Candidates.Num(), Loaded, CurrentUnitsLoaded, MaxTransportUnits);
}

void ABuildingBase::SetBeaconRange(float NewRange)
{
	BeaconRange = FMath::Max(0.f, NewRange);
}

void ABuildingBase::SpawnEnergyWall(TSubclassOf<AEnergyWall> InEnergyWallClass, ABuildingBase* InOrigin)
{
	if (!InEnergyWallClass || !InOrigin) return;

	// Requirement 2: Only connect if TeamIds are the same
	if (InOrigin->TeamId != this->TeamId) return;

	if (UWorld* World = GetWorld())
	{
		AEnergyWall* NewWall = World->SpawnActor<AEnergyWall>(InEnergyWallClass, FTransform::Identity);
		if (NewWall)
		{
			// Requirement 3: Get TeamId from Building
			NewWall->TeamId = this->TeamId;
			NewWall->Multicast_InitializeWall(InOrigin, this);
			EnergyWallArray.Add(NewWall);
			InOrigin->EnergyWallArray.Add(NewWall);
		}
	}
}

void ABuildingBase::SetEnergyWallsActive(bool bActive)
{
	for (AEnergyWall* Wall : EnergyWallArray)
	{
		if (IsValid(Wall))
		{
			if (bActive)
			{
				Wall->Multicast_ActivateWall();
			}
			else
			{
				Wall->Multicast_DeactivateWall();
			}
		}
	}
}

bool ABuildingBase::GetEnergyWallActive() const
{
	for (AEnergyWall* Wall : EnergyWallArray)
	{
		if (IsValid(Wall))
		{
			return !Wall->IsDeactivated();
		}
	}
	return false;
}

void ABuildingBase::Destroyed()
{
	// Diagnose: die Podzahl faellt messbar auch dann, wenn im gesamten Basisumkreis
	// (12000 Einheiten) kein Gegner steht - waehrend der Quelltext keinen Pfad kennt,
	// der ein Gebaeude ausser durch Kampfschaden entfernt. Einer der beiden Befunde
	// muss falsch sein, und ohne Zeitstempel + Position ist nicht zu entscheiden welcher.
	// Reine Protokollzeile, kein Verhalten.
	if (HasAuthority())
	{
		const FVector Where = GetActorLocation();
	}

	ReleaseSupplyCapacity();

	Super::Destroyed();

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	
	if(ResourceGameMode)
		ResourceGameMode->RemoveBaseFromGroup(this);
}

void ABuildingBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (ARTSGameModeBase* GM = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			GM->CheckWinLoseCondition(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ABuildingBase::SyncAttachedAssetsVisibility()
{
	Super::SyncAttachedAssetsVisibility();

	for (AEnergyWall* Wall : EnergyWallArray)
	{
		if (IsValid(Wall))
		{
			Wall->UpdateVisibility();
		}
	}
}




void ABuildingBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
	// Since Worker is already AWorkingUnitBase, no need to cast again
	AUnitBase* UnitBase = Cast<AUnitBase>(OtherActor);
	if (!UnitBase || !UnitBase->IsWorker) return;

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	if (!ResourceGameMode) return;

	// Merely brushing past a base must not adopt it as the worker's drop-off when this base
	// rejects the worker's cargo - that would send the load to a building that cannot take it.
	// The worker keeps the base it was routed to and simply walks on.
	if (!UnitBase->CanDeliverToBase(this))
	{
		return;
	}

	UnitBase->Base = this;

	bool CanAffordConstruction = false;

	if(UnitBase->BuildArea)
	{
	
		if(UnitBase->BuildArea->IsPaid)
			CanAffordConstruction = true;
		else	
			CanAffordConstruction = ResourceGameMode->CanAffordConstruction(UnitBase->BuildArea->ConstructionCost, UnitBase->TeamId);
	}
	
	if (UnitBase->IsWorker && IsBase && ResourceGameMode && UnitBase->GetUnitState() != UnitData::GoToBuild)
	{
		HandleBaseArea(UnitBase, ResourceGameMode, CanAffordConstruction);
	}

}

void ABuildingBase::MulticastRotateNiagaraToOrigin_Implementation(UNiagaraComponent* NiagaraToRotate, const FRotator& RotationOffset, float InRotateDuration, float InRotationEaseExponent, ERotationAxis AxisSelection)
{
	if (!NiagaraToRotate || !Origin)
	{
		return;
	}

	const FVector OriginLocation = Origin->GetActorLocation();
	const FVector NiagaraLocation = NiagaraToRotate->GetComponentLocation();
	const FVector Direction = OriginLocation - NiagaraLocation;

	// Calculate rotation to face the origin (aligning Z axis)
	const FRotator FaceOriginRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();

	// Apply offset
	FRotator TargetRotation = FaceOriginRotation + RotationOffset;

	// Handle axis selection
	if (AxisSelection != ERotationAxis::Full)
	{
		const FRotator CurrentRotation = NiagaraToRotate->GetRelativeRotation();
		switch (AxisSelection)
		{
		case ERotationAxis::RollPitch:
			TargetRotation.Yaw = CurrentRotation.Yaw;
			break;
		case ERotationAxis::PitchYaw:
			TargetRotation.Roll = CurrentRotation.Roll;
			break;
		case ERotationAxis::YawRoll:
			TargetRotation.Pitch = CurrentRotation.Pitch;
			break;
		default:
			break;
		}
	}

	// Call the base class function to handle the smooth rotation
	MulticastRotateNiagaraLinear(NiagaraToRotate, TargetRotation, InRotateDuration, InRotationEaseExponent);
}

bool ABuildingBase::AcceptsResourceType(EResourceType ResourceType) const
{
	// MAX means "the worker carries nothing". Such a worker is only coming home to be
	// re-dispatched, so every base must let it in - otherwise an idle worker could never
	// reach a base again.
	if (ResourceType == EResourceType::MAX)
	{
		return true;
	}

	// Unrestricted base: accepts anything. This is the default, so existing content is untouched.
	if (!bRestrictAcceptedResources)
	{
		return true;
	}

	return AcceptedResourceTypes.Contains(ResourceType);
}

void ABuildingBase::HandleBaseArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
	UnitBase->UnitControlTimer = 0;
	UnitBase->SetUEPathfinding = true;
	UnitBase->Base = this;

	// Remove from current resource place while at base to allow better redistribution
	if (AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(UnitBase))
	{
		if (IsValid(WorkingUnit->ResourcePlace))
		{
			WorkingUnit->ResourcePlace->RemoveWorkerFromArray(WorkingUnit);
		}

		if (IsValid(WorkingUnit->BuildArea))
		{
			WorkingUnit->BuildArea->RemoveWorkerFromArray(WorkingUnit);
		}
	}
	
	if (!SwitchBuildArea( UnitBase, ResourceGameMode))
	{
		SwitchResourceArea(UnitBase, ResourceGameMode);
	}
	
}

void ABuildingBase::SwitchResourceArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, int32 RecursionCount)
{
	if (!ResourceGameMode) return;

	// A held-but-depleted deposit (emptied without being destroyed, e.g. loaded from a save) is not
	// extractable. Release it up front - decrement our per-type worker slot (symmetric with the +1
	// paid on assignment) and forget it - so the logic below treats this worker as unassigned and
	// never re-registers it onto a dead deposit. GetAllResourcePlaces already filters such places.
	// Also release a deposit this worker is no longer permitted to mine (its MineableResourceTypes
	// changed, or no base of its team accepts that resource any more). Without this the "keep the
	// current resource" branch further down would pin the worker to a now-forbidden deposit forever.
	const bool bHoldsForbiddenPlace =
		IsValid(UnitBase->ResourcePlace) &&
		(!UnitBase->CanMineWorkArea(UnitBase->ResourcePlace) ||
		 !ResourceGameMode->CanTeamDeliverResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type)));

	if (IsValid(UnitBase->ResourcePlace) && (UnitBase->ResourcePlace->AvailableResourceAmount <= 0.f || bHoldsForbiddenPlace))
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
		UnitBase->ResourcePlace->RemoveWorkerFromArray(UnitBase);
		UnitBase->ResourcePlace = nullptr;
	}

	const bool bWorkerDistributionSet = ResourceGameMode->IsWorkerDistributionSet(UnitBase->TeamId);

	TArray<AWorkArea*> AllWorkPlaces = ResourceGameMode->GetAllResourcePlaces(UnitBase);
	
	if (AllWorkPlaces.Num() == 0)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Idle);
		return;
	}
	
	// Use Base location for distance calculation (worker is at base when this is called)
	const FVector BaseLocation = IsValid(UnitBase->Base) ? UnitBase->Base->GetActorLocation() : UnitBase->GetMassActorLocation();

	// Calculate distance threshold based on the closest resource (multiplier of closest distance)
	const float ClosestDistance = FVector::Dist(BaseLocation, AllWorkPlaces[0]->GetActorLocation());
	const float DistanceThreshold = ClosestDistance * ResourceGameMode->ResourceDistanceMultiplier;
	
	// Filter all work places to only include those within threshold distance
	TArray<AWorkArea*> CloseWorkPlaces;
	for (AWorkArea* WorkPlace : AllWorkPlaces)
	{
		if (!IsValid(WorkPlace)) continue;
		const float WorkPlaceDistance = FVector::Dist(BaseLocation, WorkPlace->GetActorLocation());
		if (WorkPlaceDistance <= DistanceThreshold)
		{
			CloseWorkPlaces.Add(WorkPlace);
		}
	}
	
	// If no work places within threshold, use the closest one as fallback
	if (CloseWorkPlaces.Num() == 0 && AllWorkPlaces.Num() > 0)
	{
		CloseWorkPlaces.Add(AllWorkPlaces[0]);
	}

	TArray<AWorkArea*> WorkPlacesForDistribution = bWorkerDistributionSet ? AllWorkPlaces : CloseWorkPlaces;
	
	// Get suitable work area considering worker distribution
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(UnitBase->TeamId, WorkPlacesForDistribution);

	// Check if worker should switch to a closer resource area with fewer workers
	// This ensures even distribution within the threshold
	if (IsValid(UnitBase->ResourcePlace) && CloseWorkPlaces.Num() > 0)
	{
		const float CurrentDistance = FVector::Dist(BaseLocation, UnitBase->ResourcePlace->GetActorLocation());
		const float SwitchThreshold = CurrentDistance / ResourceGameMode->ResourceDistanceMultiplier;
		
		// Collect all suitable work places that are significantly closer
		TArray<AWorkArea*> SuitableCloserAreas;
		
		for (AWorkArea* WorkPlace : CloseWorkPlaces)
		{
			if (!IsValid(WorkPlace) || WorkPlace == UnitBase->ResourcePlace)
			{
				continue;
			}
			
			const float WorkPlaceDistance = FVector::Dist(BaseLocation, WorkPlace->GetActorLocation());
			
			// Check if this work place is significantly closer (within multiplier threshold of current)
			if (WorkPlaceDistance <= SwitchThreshold)
			{
				const bool bSameType = (WorkPlace->Type == UnitBase->ResourcePlace->Type);
				
				// Same type: always allow the switch
				// Different type: check if distribution allows it
				if (bSameType)
				{
					SuitableCloserAreas.Add(WorkPlace);
				}
				else
				{
					// Different type - check distribution if set
					if (bWorkerDistributionSet)
					{
						const EResourceType WorkPlaceResourceType = ConvertToResourceType(WorkPlace->Type);
						const int32 CurrentWorkers = ResourceGameMode->GetCurrentWorkersForResourceType(UnitBase->TeamId, WorkPlaceResourceType);
						const int32 MaxWorkers = ResourceGameMode->GetMaxWorkersForResourceType(UnitBase->TeamId, WorkPlaceResourceType);
						
						// Skip this closer area if distribution doesn't allow more workers of this type
						if (CurrentWorkers >= MaxWorkers)
						{
							continue;
						}
					}
					SuitableCloserAreas.Add(WorkPlace);
				}
			}
		}
		
		// If we found suitable closer areas, pick the one with the fewest workers for even distribution
		if (SuitableCloserAreas.Num() > 0)
		{
			AWorkArea* BestWorkPlace = nullptr;
			int32 LowestWorkerCount = INT_MAX;
			
			for (AWorkArea* WorkPlace : SuitableCloserAreas)
			{
				const int32 WorkerCount = WorkPlace->Workers.Num();
				
				// Prefer areas with space
				if (WorkerCount < WorkPlace->MaxWorkerCount)
				{
					// If distribution is set, we prioritize distance among those with space
					if (bWorkerDistributionSet && !BestWorkPlace)
					{
						BestWorkPlace = WorkPlace;
						LowestWorkerCount = WorkerCount;
						// If we want absolute closest, we could return here, 
						// but balancing within the same distance class might be okay.
						// However, to be safe and satisfy "prefer proximity", let's just pick the first one (closest)
						break;
					}

					if (WorkerCount < LowestWorkerCount)
					{
						LowestWorkerCount = WorkerCount;
						BestWorkPlace = WorkPlace;
					}
				}
			}

			// Deliberately NO uncapped fallback here. This block only exists to OPTIMIZE an already
			// working assignment ("a closer deposit became free"), so if no closer deposit has room we
			// simply keep the current one. The fallback that used to sit here ignored MaxWorkerCount and
			// returned early, so a worker coming back from the base was rerouted onto a full deposit and
			// then bounced on arrival - this is the live path of the "workers stand around" report.
			
			if (BestWorkPlace)
			{
				const bool bSameType = (BestWorkPlace->Type == UnitBase->ResourcePlace->Type);
				
				if (!bSameType)
				{
					// Different type - update worker counts
					ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
					ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(BestWorkPlace->Type), +1.0f);
				}
				
				// SetResourcePlace, not a raw assignment - it releases the slot on the deposit we are
				// switching away from. A raw assignment here left a phantom worker on the old node.
				UnitBase->SetResourcePlace(BestWorkPlace, /*bRegisterOnNewPlace=*/true);

				UnitBase->SetUEPathfinding = true;
				UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
				UnitBase->SwitchEntityTagByState(UnitData::GoToResourceExtraction, UnitBase->UnitStatePlaceholder);
				return;
			}
		}
	}

	// Assign new resource place if suitable one found (already filtered by 1.5x threshold)
	if (IsValid(NewResourcePlace))
	{
		if(IsValid(UnitBase->ResourcePlace) && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		else if(!IsValid(UnitBase->ResourcePlace))
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		// Releases the previous deposit's slot before taking the new one (see above).
		UnitBase->SetResourcePlace(NewResourcePlace, /*bRegisterOnNewPlace=*/true);
	}
	else if (!IsValid(UnitBase->ResourcePlace))
	{
		// Fallback: pick the one with fewest workers from close list (extra workers)
		AWorkArea* BestFallback = nullptr;
		int32 LowestWorkerCount = INT_MAX;
		
		for (AWorkArea* WorkPlace : CloseWorkPlaces)
		{
			if (!IsValid(WorkPlace))
			{
				continue;
			}

			// For extra workers (who fall back here), we ignore the distribution type check
			// as they should just pick any close resource that has space.
			
			const int32 WorkerCount = WorkPlace->Workers.Num();
			if (WorkerCount < LowestWorkerCount && WorkerCount < WorkPlace->MaxWorkerCount)
			{
				LowestWorkerCount = WorkerCount;
				BestFallback = WorkPlace;
			}
		}

		// No uncapped second fallback here: a node at MaxWorkerCount would just bounce the worker back
		// on arrival. Leaving BestFallback null idles it instead; the AutoMining rescan retries later.

		if (BestFallback)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(BestFallback->Type), +1.0f);
			UnitBase->SetResourcePlace(BestFallback, /*bRegisterOnNewPlace=*/true);
		}
		else
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Idle);
			return;
		}
	}
	else
	{
		// Even if keeping the same resource, re-register it since we removed it in HandleBaseArea
		if (AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(UnitBase))
		{
			UnitBase->ResourcePlace->AddWorkerToArray(WorkingUnit);
		}
	}

	UnitBase->SetUEPathfinding = true;
	UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
	UnitBase->SwitchEntityTagByState(UnitData::GoToResourceExtraction, UnitBase->UnitStatePlaceholder);
}

bool ABuildingBase::SwitchBuildArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{

	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(UnitBase);

	if (BuildAreas.Num() > 3) BuildAreas.SetNum(3);
	
	UnitBase->BuildArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas); // BuildAreas.Num() ? BuildAreas[0] : nullptr;

	bool CanAffordConstruction = false;

	if(UnitBase->BuildArea && UnitBase->BuildArea->IsPaid)
		CanAffordConstruction = true;
	else	
		CanAffordConstruction = UnitBase->BuildArea? ResourceGameMode->CanAffordConstruction(UnitBase->BuildArea->ConstructionCost, UnitBase->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	
	bool AreaIsForTeam = false;
	if (UnitBase->BuildArea)
	{
		AreaIsForTeam = 
			(UnitBase->BuildArea->TeamId == 0) || 
			(UnitBase->TeamId == UnitBase->BuildArea->TeamId);
	}

	if(CanAffordConstruction && UnitBase->BuildArea && !UnitBase->BuildArea->PlannedBuilding && AreaIsForTeam) // && AreaIsForTeam
	{
	
		UnitBase->BuildArea->PlannedBuilding = true;
		UnitBase->BuildArea->ControlTimer = 0.f;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToBuild);
		UnitBase->SwitchEntityTagByState(UnitData::GoToBuild, UnitBase->UnitStatePlaceholder);
		return true;
	}
	
	return false;

}

void ABuildingBase::DespawnWorkResource(AWorkResource* ResourceToDespawn)
{
	if (ResourceToDespawn != nullptr)
	{
		ResourceToDespawn->Destroy();
		ResourceToDespawn = nullptr;
	}
}


void ABuildingBase::SetEnemyVisibility(AActor* DetectingActor, bool bVisible)
{
	if (ISMComponent && RunTimeCustomDepthSwitch && !IsMyTeam)
	{
		if (!bUseSkeletalMovement)
		{
			UInstancedStaticMeshComponent* TargetISM = nullptr;
			int32 TargetInstIndex = INDEX_NONE;
			if (GetMassVisualInstance(ISMComponent, TargetISM, TargetInstIndex))
			{
				if (TargetISM)
				{
					TargetISM->SetRenderCustomDepth(bVisible);
				}
			}
		}else
		{
			if (USkeletalMeshComponent* SKM = GetMesh())
			{
				SKM->SetRenderCustomDepth(bVisible);
			}
		}
	}

	if (!CanMove && bVisible == false) return;
	Super::SetEnemyVisibility(DetectingActor, bVisible);
}

bool ABuildingBase::IsInBeaconRange() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	return ABuildingBase::IsLocationInBeaconRange(World, GetActorLocation());
}

bool ABuildingBase::IsLocationInBeaconRange(UWorld* World, const FVector& Location)
{
	if (URTSBeaconSubsystem* BeaconSubsystem = World ? World->GetSubsystem<URTSBeaconSubsystem>() : nullptr)
	{
		return BeaconSubsystem->IsLocationInBeaconRange(Location);
	}
	return false;
}

