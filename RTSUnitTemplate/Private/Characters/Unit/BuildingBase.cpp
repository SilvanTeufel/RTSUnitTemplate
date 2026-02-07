// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/BuildingBase.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameModes/ResourceGameMode.h"
#include "GameModes/RTSGameModeBase.h"
#include "Components/CapsuleComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "EngineUtils.h"


ABuildingBase::ABuildingBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Buildings are stationary and may use ISMs for visuals; no dedicated SnapMesh is needed.
	CanMove = false;
}

void ABuildingBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(ResourceGameMode)
		ResourceGameMode->AddBaseToGroup(this);
}

void ABuildingBase::SetBeaconRange(float NewRange)
{
	BeaconRange = FMath::Max(0.f, NewRange);
}

void ABuildingBase::Destroyed()
{
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




void ABuildingBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
	// Since Worker is already AWorkingUnitBase, no need to cast again
	AUnitBase* UnitBase = Cast<AUnitBase>(OtherActor);
	if (!UnitBase || !UnitBase->IsWorker) return;
	
	UnitBase->Base = this;
	
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	if (!ResourceGameMode) return;
	
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
	}
	
	if(UnitBase->WorkResource)
	{
		//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
		ResourceGameMode->ModifyResource(UnitBase->WorkResource->ResourceType, UnitBase->TeamId, UnitBase->WorkResource->Amount);
		DespawnWorkResource(UnitBase->WorkResource);
	}
	
	
	if (!SwitchBuildArea( UnitBase, ResourceGameMode))
	{
		SwitchResourceArea(UnitBase, ResourceGameMode);
	}
	
}

void ABuildingBase::SwitchResourceArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, int32 RecursionCount)
{
	if (!ResourceGameMode) return;

	TArray<AWorkArea*> AllWorkPlaces = ResourceGameMode->GetClosestResourcePlaces(UnitBase);
	
	if (AllWorkPlaces.Num() == 0)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Idle);
		return;
	}
	
	// Use Base location for distance calculation (worker is at base when this is called)
	const FVector BaseLocation = IsValid(UnitBase->Base) ? UnitBase->Base->GetActorLocation() : UnitBase->GetActorLocation();
	const bool bWorkerDistributionSet = ResourceGameMode->IsWorkerDistributionSet(UnitBase->TeamId);
	
	// Calculate distance threshold based on the closest resource (multiplier of closest distance)
	const float ClosestDistance = FVector::Dist(BaseLocation, AllWorkPlaces[0]->GetActorLocation());
	const float DistanceThreshold = ClosestDistance * ResourceGameMode->ResourceDistanceMultiplier;
	
	// Filter all work places to only include those within threshold distance
	TArray<AWorkArea*> WorkPlaces;
	for (AWorkArea* WorkPlace : AllWorkPlaces)
	{
		if (!IsValid(WorkPlace))
		{
			continue;
		}
		
		const float WorkPlaceDistance = FVector::Dist(BaseLocation, WorkPlace->GetActorLocation());
		if (WorkPlaceDistance <= DistanceThreshold)
		{
			WorkPlaces.Add(WorkPlace);
		}
	}
	
	// If no work places within threshold, use the closest one as fallback
	if (WorkPlaces.Num() == 0 && AllWorkPlaces.Num() > 0)
	{
		WorkPlaces.Add(AllWorkPlaces[0]);
	}
	
	// Get suitable work area considering worker distribution
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(UnitBase->TeamId, WorkPlaces);

	// Check if worker should switch to a closer resource area with fewer workers
	// This ensures even distribution within the threshold
	if (IsValid(UnitBase->ResourcePlace) && WorkPlaces.Num() > 0)
	{
		const float CurrentDistance = FVector::Dist(BaseLocation, UnitBase->ResourcePlace->GetActorLocation());
		const float SwitchThreshold = CurrentDistance / ResourceGameMode->ResourceDistanceMultiplier;
		
		// Collect all suitable work places that are significantly closer
		TArray<AWorkArea*> SuitableCloserAreas;
		
		for (AWorkArea* WorkPlace : WorkPlaces)
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
				if (WorkerCount < LowestWorkerCount)
				{
					LowestWorkerCount = WorkerCount;
					BestWorkPlace = WorkPlace;
				}
			}
			
			if (BestWorkPlace)
			{
				const bool bSameType = (BestWorkPlace->Type == UnitBase->ResourcePlace->Type);
				
				if (!bSameType)
				{
					// Different type - update worker counts
					ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
					ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(BestWorkPlace->Type), +1.0f);
				}
				
				UnitBase->ResourcePlace = BestWorkPlace;
				
				// Register worker at the new location immediately
				if (AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(UnitBase))
				{
					UnitBase->ResourcePlace->AddWorkerToArray(WorkingUnit);
				}
				
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
		UnitBase->ResourcePlace = NewResourcePlace;

		// Register worker at the new location immediately
		if (AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(UnitBase))
		{
			UnitBase->ResourcePlace->AddWorkerToArray(WorkingUnit);
		}
	}
	else if (!IsValid(UnitBase->ResourcePlace))
	{
		// Fallback: pick the one with fewest workers from filtered list
		AWorkArea* BestFallback = nullptr;
		int32 LowestWorkerCount = INT_MAX;
		
		for (AWorkArea* WorkPlace : WorkPlaces)
		{
			if (!IsValid(WorkPlace))
			{
				continue;
			}
			
			const int32 WorkerCount = WorkPlace->Workers.Num();
			if (WorkerCount < LowestWorkerCount)
			{
				LowestWorkerCount = WorkerCount;
				BestFallback = WorkPlace;
			}
		}
		
		if (BestFallback)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(BestFallback->Type), +1.0f);
			UnitBase->ResourcePlace = BestFallback;

			// Register worker at the fallback location immediately
			if (AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(UnitBase))
			{
				UnitBase->ResourcePlace->AddWorkerToArray(WorkingUnit);
			}
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

void ABuildingBase::MulticastSetEnemyVisibility_Implementation(APerformanceUnit* DetectingActor, bool bVisible)
{
	if (!CanMove && bVisible == false) return;
	Super::MulticastSetEnemyVisibility_Implementation(DetectingActor, bVisible);
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
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		ABuildingBase* Beacon = *It;
		if (!Beacon)
		{
			continue;
		}
		if (Beacon->BeaconRange > 0.f)
		{
			const float Dist2D = FVector::Dist2D(Beacon->GetActorLocation(), Location);
			if (Dist2D <= Beacon->BeaconRange)
			{
				return true;
			}
		}
	}
	return false;
}

