// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/ResourceGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h" // For TActorIterator
#include "Actors/WorkArea.h"
#include "Actors/WinLoseConfigActor.h"
#include "Characters/Unit/BuildingBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameStates/ResourceGameState.h"
#include "Net/UnrealNetwork.h"


#include "System/MapSwitchSubsystem.h"
#include "Engine/GameInstance.h"

AResourceGameMode::AResourceGameMode()
{
	ResourceDistanceMultiplier = 2.0f;
	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources); 
	}
}

void AResourceGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Initialize resources for the game
	InitializeResources(NumberOfTeams);
	GatherWorkAreas();
	//GatherBases();
	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources); 
	}
	//AssignWorkAreasToWorkers();
}

void AResourceGameMode::CheckWinLoseConditionTimer()
{
	CheckWinLoseCondition(nullptr);
}

void AResourceGameMode::CheckWinLoseCondition(AUnitBase* DestroyedUnit)
{
	Super::CheckWinLoseCondition(DestroyedUnit);
}

void AResourceGameMode::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AResourceGameMode, TeamResources);
	DOREPLIFETIME(AResourceGameMode, NumberOfTeams);
}

void AResourceGameMode::InitializeResources(int32 InNumberOfTeams)
{
	NumberOfTeams = InNumberOfTeams;
	TeamResources.Empty();

	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->IsSupplyLike.SetNumZeroed(static_cast<int32>(EResourceType::MAX));
	}

	for(int32 ResourceTypeIndex = 0; ResourceTypeIndex < static_cast<int32>(EResourceType::MAX); ++ResourceTypeIndex)
	{
		EResourceType ResourceType = static_cast<EResourceType>(ResourceTypeIndex);
		TeamResources.Add(FResourceArray(ResourceType, NumberOfTeams));

		if (RGState)
		{
			bool bIsSupply = SupplyLikeResources.Contains(ResourceType) ? SupplyLikeResources[ResourceType] : false;
			RGState->IsSupplyLike[ResourceTypeIndex] = bIsSupply;
		}
	}
}
void AResourceGameMode::GatherBases()
{
	for (TActorIterator<ABuildingBase> It(GetWorld()); It; ++It)
	{
		ABuildingBase* BuildingBase = *It;
		if (!BuildingBase) continue;

		if(BuildingBase->IsBase)
		{
			WorkAreaGroups.BaseAreas.Add(BuildingBase);
		}
	}

}

void AResourceGameMode::AddBaseToGroup(ABuildingBase* BuildingBase)
{
	if(BuildingBase && BuildingBase->IsBase && BuildingBase->GetUnitState() != UnitData::Dead)
		WorkAreaGroups.BaseAreas.Add(BuildingBase);
}

void AResourceGameMode::RemoveBaseFromGroup(ABuildingBase* BuildingBase)
{
		if(BuildingBase && BuildingBase->IsBase && BuildingBase->GetUnitState() == UnitData::Dead)
			WorkAreaGroups.BaseAreas.Remove(BuildingBase);
}

void AResourceGameMode::GatherWorkAreas()
{
	UE_LOG(LogTemp, Warning, TEXT("GatherWorkAreas"));
	for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
	{
		AWorkArea* WorkArea = *It;
		if (!WorkArea) continue;

		switch (WorkArea->Type)
		{
		case WorkAreaData::Primary:
			WorkAreaGroups.PrimaryAreas.Add(WorkArea);
			break;
		case WorkAreaData::Secondary:
			WorkAreaGroups.SecondaryAreas.Add(WorkArea);
			break;
		case WorkAreaData::Tertiary:
			WorkAreaGroups.TertiaryAreas.Add(WorkArea);
			break;
		case WorkAreaData::Rare:
			WorkAreaGroups.RareAreas.Add(WorkArea);
			break;
		case WorkAreaData::Epic:
			WorkAreaGroups.EpicAreas.Add(WorkArea);
			break;
		case WorkAreaData::Legendary:
			WorkAreaGroups.LegendaryAreas.Add(WorkArea);
			break;
		/*case WorkAreaData::Base:
			WorkAreaGroups.BaseAreas.Add(WorkArea);
			break;*/
		case WorkAreaData::BuildArea:
			WorkAreaGroups.BuildAreas.Add(WorkArea);
			break;
		default:
			// Handle any cases not explicitly covered
			break;
		}
	}
	
}

// Adjusting the ModifyResource function to use the ResourceType within FResourceArray
void AResourceGameMode::ModifyResource_Implementation(EResourceType ResourceType, int32 TeamId, float Amount)
{
	for (FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (TeamId >= 0 && TeamId < ResourceArray.Resources.Num())
			{
				bool bIsSupply = SupplyLikeResources.Contains(ResourceType) ? SupplyLikeResources[ResourceType] : false;
				if (bIsSupply)
				{
					// A supply-like resource counts what is USED, so it can never be less than
					// nothing. Without this floor a refund that no charge matched showed up in the
					// UI as "-55/70". The clamp is the safety net, not the fix - it logs, so an
					// imbalance stays visible instead of being silently absorbed.
					const float Before = ResourceArray.Resources[TeamId];
					const float Wanted = Before - Amount; // Inverted logic for Supply
					if (Wanted < 0.f)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("[Versorgung] Team %d: Rueckgabe von %.0f wuerde den Verbrauch auf %.0f druecken (vorher %.0f) - auf 0 begrenzt."),
							TeamId, Amount, Wanted, Before);
					}
					ResourceArray.Resources[TeamId] = FMath::Max(0.f, Wanted);
				}
				else
				{
					ResourceArray.Resources[TeamId] += Amount;
				}
				break; // Exit once the correct resource type is modified
			}
		}
	}

	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources);
	}
	CheckWinLoseCondition();
}

void AResourceGameMode::ModifyMaxResource_Implementation(EResourceType ResourceType, int32 TeamId, float Amount)
{
	for (FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (ResourceArray.MaxResources.IsValidIndex(TeamId))
			{
				ResourceArray.MaxResources[TeamId] = FMath::Clamp(ResourceArray.MaxResources[TeamId] + Amount, 0.0f, HighestMaxResource);
				break;
			}
		}
	}

	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources);
	}
}

void AResourceGameMode::IncreaseMaxResources(const FBuildingCost& CapacityIncrease, int32 TeamId)
{
	ModifyMaxResource(EResourceType::Primary, TeamId, (float)CapacityIncrease.PrimaryCost);
	ModifyMaxResource(EResourceType::Secondary, TeamId, (float)CapacityIncrease.SecondaryCost);
	ModifyMaxResource(EResourceType::Tertiary, TeamId, (float)CapacityIncrease.TertiaryCost);
	ModifyMaxResource(EResourceType::Rare, TeamId, (float)CapacityIncrease.RareCost);
	ModifyMaxResource(EResourceType::Epic, TeamId, (float)CapacityIncrease.EpicCost);
	ModifyMaxResource(EResourceType::Legendary, TeamId, (float)CapacityIncrease.LegendaryCost);
}

void AResourceGameMode::DecreaseMaxResources(const FBuildingCost& CapacityDecrease, int32 TeamId)
{
	ModifyMaxResource(EResourceType::Primary, TeamId, -(float)CapacityDecrease.PrimaryCost);
	ModifyMaxResource(EResourceType::Secondary, TeamId, -(float)CapacityDecrease.SecondaryCost);
	ModifyMaxResource(EResourceType::Tertiary, TeamId, -(float)CapacityDecrease.TertiaryCost);
	ModifyMaxResource(EResourceType::Rare, TeamId, -(float)CapacityDecrease.RareCost);
	ModifyMaxResource(EResourceType::Epic, TeamId, -(float)CapacityDecrease.EpicCost);
	ModifyMaxResource(EResourceType::Legendary, TeamId, -(float)CapacityDecrease.LegendaryCost);
}

bool AResourceGameMode::IsSupplyLikeResource(EResourceType ResourceType) const
{
	const bool* Found = SupplyLikeResources.Find(ResourceType);
	return Found ? *Found : false;
}

float AResourceGameMode::GetMaxResource(EResourceType ResourceType, int TeamId)
{
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (ResourceArray.MaxResources.IsValidIndex(TeamId))
			{
				return ResourceArray.MaxResources[TeamId];
			}
		}
	}
	return 0.0f;
}

bool AResourceGameMode::CanAffordConstruction(const FBuildingCost& ConstructionCost, int32 TeamId) const
{

	if (TeamResources.Num() == 0 || TeamId < 0 || TeamId >= NumberOfTeams)
		return false;

	// Initialize a map to store the total costs for easy comparison
	TMap<EResourceType, int32> Costs;
	Costs.Add(EResourceType::Primary, ConstructionCost.PrimaryCost);
	Costs.Add(EResourceType::Secondary, ConstructionCost.SecondaryCost);
	Costs.Add(EResourceType::Tertiary, ConstructionCost.TertiaryCost);
	Costs.Add(EResourceType::Rare, ConstructionCost.RareCost);
	Costs.Add(EResourceType::Epic, ConstructionCost.EpicCost);
	Costs.Add(EResourceType::Legendary, ConstructionCost.LegendaryCost);

	// Verify resources for each type
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		// Ensure TeamId is within bounds for the resource array
		if (!ResourceArray.Resources.IsValidIndex(TeamId))
		{
			return false; // This ensures we don't proceed with invalid TeamId
		}

		float ResourceAmount = ResourceArray.Resources[TeamId];

		// Check if the team has enough resources of the current type
		if (Costs.Contains(ResourceArray.ResourceType) && Costs[ResourceArray.ResourceType] > 0)
		{
			bool bIsSupply = SupplyLikeResources.FindRef(ResourceArray.ResourceType);
			if (bIsSupply)
			{
				if (ResourceAmount + Costs[ResourceArray.ResourceType] > ResourceArray.MaxResources[TeamId])
				{
					return false;
				}
			}
			else if (ResourceAmount < Costs[ResourceArray.ResourceType])
			{
				return false; // Not enough resources of this type
			}
		}
	}

	// If all costs are affordable
	return true;
}

bool AResourceGameMode::CanAffordResource(EResourceType ResourceType, float Amount, int32 TeamId) const
{
	if (TeamId < 0 || TeamId >= NumberOfTeams)
		return false;

	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (!ResourceArray.Resources.IsValidIndex(TeamId))
				return false;

			float ResourceAmount = ResourceArray.Resources[TeamId];
			bool bIsSupply = SupplyLikeResources.FindRef(ResourceType);

			if (bIsSupply)
			{
				// For supply-like resources, check if adding the amount exceeds max capacity
				return (ResourceAmount + Amount <= ResourceArray.MaxResources[TeamId]);
			}
			else
			{
				// For standard resources, check if the current amount is sufficient
				return (ResourceAmount >= Amount);
			}
		}
	}
	return false;
}

bool AResourceGameMode::CanAffordConstructionExtended(const FBuildingCost& ConstructionCost, int32 TeamId, TArray<EResourceType>& OutMissingResources) const
{
	OutMissingResources.Empty();

	if (TeamResources.Num() == 0 || TeamId < 0 || TeamId >= NumberOfTeams)
		return false;

	// Map the FBuildingCost members to EResourceType for easier iteration
	TMap<EResourceType, int32> Costs;
	Costs.Add(EResourceType::Primary, ConstructionCost.PrimaryCost);
	Costs.Add(EResourceType::Secondary, ConstructionCost.SecondaryCost);
	Costs.Add(EResourceType::Tertiary, ConstructionCost.TertiaryCost);
	Costs.Add(EResourceType::Rare, ConstructionCost.RareCost);
	Costs.Add(EResourceType::Epic, ConstructionCost.EpicCost);
	Costs.Add(EResourceType::Legendary, ConstructionCost.LegendaryCost);

	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (!ResourceArray.Resources.IsValidIndex(TeamId))
			continue;

		float ResourceAmount = ResourceArray.Resources[TeamId];
		EResourceType RType = ResourceArray.ResourceType;

		if (Costs.Contains(RType) && Costs[RType] > 0)
		{
			bool bIsSupply = SupplyLikeResources.FindRef(RType);
			bool bAffordable = true;

			if (bIsSupply)
			{
				if (ResourceAmount + Costs[RType] > ResourceArray.MaxResources[TeamId])
				{
					bAffordable = false;
				}
			}
			else if (ResourceAmount < Costs[RType])
			{
				bAffordable = false;
			}

			if (!bAffordable)
			{
				OutMissingResources.Add(RType);
			}
		}
	}

	// Returns true if no resources were missing
	return OutMissingResources.Num() == 0;
}

void AResourceGameMode::AssignWorkAreasToWorkers()
{
	TArray<AActor*> TempActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorkingUnitBase::StaticClass(), TempActors);
	
	// Use the improved AssignWorkAreasToWorker function for each worker
	// This ensures proper distribution with DistanceThresholdMultiplier and even worker distribution
	for (AActor* MyActor : TempActors)
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(MyActor);
		
		if (!Worker || !Worker->IsWorker) continue;

		// Use the single worker assignment function which handles:
		// - DistanceThresholdMultiplier threshold from base
		// - Worker distribution constraints (TeamResources)
		// - Even distribution based on worker count at each location
		AssignWorkAreasToWorker(Worker);
	}
}

void AResourceGameMode::AssignWorkAreasToWorker(AWorkingUnitBase* Worker)
{
	
	if (!Worker || !Worker->IsWorker) return;

	// Assign the closest base
	Worker->Base = GetClosestBaseFromArray(Worker, WorkAreaGroups.BaseAreas);

	// Get the closest resource places (sorted by distance to base)
	TArray<AWorkArea*> WorkPlaces = GetFiveClosestResourcePlaces(Worker);
	
	if (WorkPlaces.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No resource places available for Worker: %s"), *Worker->GetName());
		return;
	}
	
	// Check if worker distribution is set for this team
	const bool bWorkerDistributionSet = IsWorkerDistributionSet(Worker->TeamId);
	
	// Filter work places based on worker distribution settings and find the one with fewest workers
	AWorkArea* BestWorkPlace = nullptr;
	
	// Get the closest resource's distance as reference for threshold
	const FVector ReferenceLocation = (Worker->Base && IsValid(Worker->Base)) 
		? Worker->Base->GetActorLocation() 
		: Worker->GetActorLocation();
	
	const float ClosestDistance = WorkPlaces.Num() > 0 
		? FVector::Dist(ReferenceLocation, WorkPlaces[0]->GetActorLocation()) 
		: 0.f;
	const float DistanceThreshold = ClosestDistance * ResourceDistanceMultiplier;
	
	for (AWorkArea* WorkPlace : WorkPlaces)
	{
		if (!IsValid(WorkPlace))
		{
			continue;
		}
		
		// Only consider work places within multiplier distance of the closest one
		const float WorkPlaceDistance = FVector::Dist(ReferenceLocation, WorkPlace->GetActorLocation());
		if (WorkPlaceDistance > DistanceThreshold)
		{
			continue;
		}
		
		// Check worker distribution constraints if set
		if (bWorkerDistributionSet)
		{
			const EResourceType ResourceType = ConvertToResourceType(WorkPlace->Type);
			const int32 CurrentWorkers = GetCurrentWorkersForResourceType(Worker->TeamId, ResourceType);
			const int32 MaxWorkers = GetMaxWorkersForResourceType(Worker->TeamId, ResourceType);
			
			// Skip if this resource type has reached its worker limit
			if (CurrentWorkers >= MaxWorkers)
			{
				continue;
			}
		}
		
		// Never hand out a node that is already at its per-node cap - a worker sent there just bounces
		// off ReserveMiningSlotOrReassign on arrival. (Workers.Contains means WE already hold a slot.)
		if (!AWorkArea::HasFreeMiningSlotFor(WorkPlace, Worker))
		{
			continue;
		}

		// Naechstes FREIES Feld gewinnt. WorkPlaces ist nach Entfernung zur Basis sortiert; die
		// vorherige Wahl "wenigste Arbeiter" hat diese Sortierung ignoriert und schickte Arbeiter
		// an ein leeres Feld quer ueber die Karte, obwohl direkt nebenan noch ein Platz frei war.
		// Volle Felder sind oben schon aussortiert, weiter weg wird also nur gelaufen, wenn in der
		// Naehe wirklich nichts frei ist.
		BestWorkPlace = WorkPlace;
		break;
	}

	// Fallback: if no suitable place found with distribution constraints, try without constraints
	if (!BestWorkPlace && bWorkerDistributionSet)
	{
		for (AWorkArea* WorkPlace : WorkPlaces)
		{
			if (!IsValid(WorkPlace))
			{
				continue;
			}

			const float WorkPlaceDistance = FVector::Dist(ReferenceLocation, WorkPlace->GetActorLocation());
			if (WorkPlaceDistance > DistanceThreshold)
			{
				continue;
			}

			// The per-TYPE quota may be relaxed here, but the per-NODE cap still holds.
			if (!AWorkArea::HasFreeMiningSlotFor(WorkPlace, Worker))
			{
				continue;
			}

			// Auch hier: das naechste freie Feld, nicht das am wenigsten ausgelastete.
			BestWorkPlace = WorkPlace;
			break;
		}
	}

	// Final fallback: the closest node that still has room. Deliberately NOT "just WorkPlaces[0]" any
	// more: taking a full node here is what made freshly spawned workers walk to a 3/3 deposit and then
	// stall. If every candidate is full the worker stays unassigned and idles - SynchronizeUnitState's
	// AutoMining rescan retries later, when a slot has freed up.
	if (!BestWorkPlace)
	{
		for (AWorkArea* WorkPlace : WorkPlaces)
		{
			if (AWorkArea::HasFreeMiningSlotFor(WorkPlace, Worker))
			{
				BestWorkPlace = WorkPlace;
				break;
			}
		}
	}
	
	// Pay back the previous assignment before overwriting it. This function runs TWICE for every
	// worker - once from AWorkingUnitBase::BeginPlay and once from the UnitSpawned signal
	// (UUnitStateProcessor::HandleUnitSpawnedSignal) - and because the selection prefers the node with
	// the fewest workers, the second run usually picks a DIFFERENT node than the first. Without this
	// release the first node keeps a phantom worker (and a phantom per-type count) forever, which is
	// exactly the "2/3 with nobody mining" the HUD shows on a freshly started map.
	if (IsValid(Worker->ResourcePlace) && Worker->ResourcePlace != BestWorkPlace)
	{
		AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Worker->ResourcePlace->Type), -1.0f);
	}

	const bool bWasAlreadyAssignedHere = (Worker->ResourcePlace == BestWorkPlace);
	Worker->SetResourcePlace(BestWorkPlace);

	if (Worker->ResourcePlace)
	{
		// Update the CurrentWorkers count for the assigned resource type
		// This ensures proper distribution when multiple workers are assigned in sequence
		if (!bWasAlreadyAssignedHere)
		{
			const EResourceType AssignedResourceType = ConvertToResourceType(Worker->ResourcePlace->Type);
			AddCurrentWorkersForResourceType(Worker->TeamId, AssignedResourceType, +1.0f);
		}

		// Add worker to the WorkArea's worker list immediately to inform subsequent assignments
		Worker->ResourcePlace->AddWorkerToArray(Worker);

		// The base above was picked before we knew which resource this worker would gather, so at
		// that point GetClosestBaseFromArray could not filter by cargo. Now that the deposit is
		// known, re-pick the nearest base that actually accepts it. Only replace the current base
		// if we find one - a null result would strand the worker.
		if (ABuildingBase* AcceptingBase = GetClosestBaseFromArray(Worker, WorkAreaGroups.BaseAreas))
		{
			Worker->Base = AcceptingBase;
		}
	}

}

ABuildingBase* AResourceGameMode::GetClosestBaseFromArray(AWorkingUnitBase* Worker, const TArray<ABuildingBase*>& Bases)
{
    if (!IsValid(Worker))
    {
        return nullptr;
    }

    ABuildingBase* ClosestBase = nullptr;
    float MinDistanceSquared = FLT_MAX;

    // Only bases that accept what this worker carries (or is about to fetch) are eligible, so a
    // restricted worker never walks to a drop-off that would reject its load.
    const EResourceType RoutingType = GetWorkerRoutingResourceType(Worker);

    // The 'Bases' array is iterated over.
    for (ABuildingBase* Base : Bases)
    {
       if (IsValid(Base) && !Base->AcceptsResourceType(RoutingType))
       {
          continue;
       }

       if (IsValid(Base) && Worker->TeamId == Base->TeamId && Base->GetUnitState() != UnitData::Dead)
       {
			if (Worker->ResourcePlace)
			{
				float DistanceSquared = (Base->GetActorLocation() - Worker->ResourcePlace->GetActorLocation()).SizeSquared();
				if (DistanceSquared < MinDistanceSquared)
				{
					MinDistanceSquared = DistanceSquared;
					ClosestBase = Base;
				}
			}	
			else
			{
				float DistanceSquared = (Base->GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
				if (DistanceSquared < MinDistanceSquared)
				{
					MinDistanceSquared = DistanceSquared;
					ClosestBase = Base;
				}
			}
       }
    }

    return ClosestBase;
}

AWorkArea* AResourceGameMode::GetClosestWorkArea(AWorkingUnitBase* Worker, const TArray<AWorkArea*>& WorkAreas)
{
	AWorkArea* ClosestArea = nullptr;
	float MinDistanceSquared = FLT_MAX;

	for (AWorkArea* Area : WorkAreas)
	{
		float DistanceSquared = (Area->GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
		if (DistanceSquared < MinDistanceSquared)
		{
			MinDistanceSquared = DistanceSquared;
			ClosestArea = Area;
		}
	}

	return ClosestArea;
}

TArray<AWorkArea*> AResourceGameMode::GetFiveClosestResourcePlaces(AWorkingUnitBase* Worker)
{
	
	WorkAreaGroups.PrimaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.SecondaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.TertiaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.RareAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.EpicAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.LegendaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	
	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.PrimaryAreas);
	AllAreas.Append(WorkAreaGroups.SecondaryAreas);
	AllAreas.Append(WorkAreaGroups.TertiaryAreas);
	AllAreas.Append(WorkAreaGroups.RareAreas);
	AllAreas.Append(WorkAreaGroups.EpicAreas);
	AllAreas.Append(WorkAreaGroups.LegendaryAreas);
	// Exclude BaseAreas and BuildAreas if they are not considered resource places

	// Skip depleted resource areas (AvailableResourceAmount <= 0) and any stale/invalid pointers so
	// workers are never assigned to a place they cannot actually extract from.
	AllAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area) || Area->AvailableResourceAmount <= 0.f; });

	// Same permission filter as GetAllResourcePlaces - this list is the one AssignWorkAreasToWorker
	// uses for the initial assignment, and it builds its own copy instead of reusing that helper.
	FilterResourcePlacesForWorker(Worker, AllAreas);

	// Sort all areas by distance to the worker's base (if available), otherwise use worker location
	// This ensures resources are selected based on proximity to the base, not the worker's current position
	const FVector ReferenceLocation = (Worker->Base && IsValid(Worker->Base))
		? Worker->Base->GetActorLocation()
		: Worker->GetActorLocation();
	
	AllAreas.Sort([ReferenceLocation](const AWorkArea& AreaA, const AWorkArea& AreaB) {
		return (AreaA.GetActorLocation() - ReferenceLocation).SizeSquared() < 
			   (AreaB.GetActorLocation() - ReferenceLocation).SizeSquared();
	});

	// Take up to the first five areas
	int32 NumAreas = FMath::Min(MaxResourceAreasToSet, AllAreas.Num());
	TArray<AWorkArea*> ClosestAreas;
	for (int i = 0; i < NumAreas; ++i)
	{
		ClosestAreas.Add(AllAreas[i]);
	}

	return ClosestAreas;
}

AWorkArea* AResourceGameMode::GetRandomClosestWorkArea(const TArray<AWorkArea*>& WorkAreas)
{
	if (WorkAreas.Num() > 0)
	{
		int32 Index = FMath::RandRange(0, WorkAreas.Num() - 1);
		return WorkAreas[Index];
	}
	
	return nullptr;
}

TArray<AWorkArea*> AResourceGameMode::GetClosestBuildPlaces(AWorkingUnitBase* Worker)
{
	// Clean up all arrays in WorkAreaGroups to remove invalid pointers
	WorkAreaGroups.BuildAreas.RemoveAll([](const AWorkArea* Area) { return !IsValid(Area); });
    
	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.BuildAreas);
	// Exclude BaseAreas and BuildAreas if they are not considered resource places

	// Remove null pointers from AllAreas
	AllAreas.RemoveAll([](AWorkArea* Area) { return Area == nullptr; });

	// Sort all areas by distance to the worker
	AllAreas.Sort([Worker](const AWorkArea& AreaA, const AWorkArea& AreaB) {
		return (AreaA.GetActorLocation() - Worker->GetActorLocation()).SizeSquared() < 
			   (AreaB.GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
	});

	// Take up to the first X areas
	int32 NumAreas = FMath::Min(MaxBuildAreasToSet, AllAreas.Num());

	TArray<AWorkArea*> ClosestAreas;
	
	for (int i = 0; i < NumAreas; ++i)
	{
		if(Worker)
		{
			if(!AllAreas[i]->PlannedBuilding && !AllAreas[i]->IsExtensionArea && (AllAreas[i]->TeamId == Worker->TeamId || AllAreas[i]->TeamId == 0))
			{
				ClosestAreas.Add(AllAreas[i]);
			}
		}
	}

	AllAreas.Empty();

	return ClosestAreas;
}


bool AResourceGameMode::ModifyResourceCCost(const FBuildingCost& ConstructionCost, int32 TeamId) // Remove trailing 'const'
{
	// Validate team and resources
	if (TeamResources.Num() == 0 || TeamId < 0 || TeamId >= NumberOfTeams)
		return false;

	// Check affordability first
	if (!CanAffordConstruction(ConstructionCost, TeamId))
		return false;

	// Create cost mapping
	TMap<EResourceType, int32> CostMap;
	CostMap.Add(EResourceType::Primary, ConstructionCost.PrimaryCost);
	CostMap.Add(EResourceType::Secondary, ConstructionCost.SecondaryCost);
	CostMap.Add(EResourceType::Tertiary, ConstructionCost.TertiaryCost);
	CostMap.Add(EResourceType::Rare, ConstructionCost.RareCost);
	CostMap.Add(EResourceType::Epic, ConstructionCost.EpicCost);
	CostMap.Add(EResourceType::Legendary, ConstructionCost.LegendaryCost);

	// Deduct resources using NON-CONST references
	for (FResourceArray& ResourceArray : TeamResources) // Remove 'const'
	{
		if (CostMap.Contains(ResourceArray.ResourceType))
		{
			if (!ResourceArray.Resources.IsValidIndex(TeamId))
			{
				UE_LOG(LogTemp, Error, TEXT("Invalid TeamID %d for resource deduction!"), TeamId);
				return false;
			}

			// Now allowed to modify
			bool bIsSupply = SupplyLikeResources.Contains(ResourceArray.ResourceType) ? SupplyLikeResources[ResourceArray.ResourceType] : false;
			if (bIsSupply)
			{
				// Same floor as in ModifyResource: a refund (negative cost, e.g. bRefundOnCancel)
				// must not push the used amount below zero.
				const float Wanted = ResourceArray.Resources[TeamId] + CostMap[ResourceArray.ResourceType];
				if (Wanted < 0.f)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[Versorgung] Team %d: Kostenerstattung wuerde den Verbrauch auf %.0f druecken - auf 0 begrenzt."),
						TeamId, Wanted);
				}
				ResourceArray.Resources[TeamId] = FMath::Max(0.f, Wanted);
			}
			else
			{
				ResourceArray.Resources[TeamId] -= CostMap[ResourceArray.ResourceType];
			}
		}
	}

	// Update game state
	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources);
	}

	CheckWinLoseCondition();

	return true;
}

float AResourceGameMode::GetResource(int32 TeamId, EResourceType ResourceType) const
{
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.Resources.IsValidIndex(TeamId) && ResourceArray.ResourceType == ResourceType)
		{
			return ResourceArray.Resources[TeamId];
		}
	}
	return 0;
}

TArray<AWorkArea*> AResourceGameMode::GetClosestResourcePlaces(AWorkingUnitBase* Worker)
{
	TArray<AWorkArea*> AllAreas = GetAllResourcePlaces(Worker);

	// Take up to the first five areas
	int32 NumAreas = FMath::Min(MaxResourceAreasToSet, AllAreas.Num());
	TArray<AWorkArea*> ClosestAreas;
	for (int i = 0; i < NumAreas; ++i)
	{
		ClosestAreas.Add(AllAreas[i]);
	}

	return ClosestAreas;
}

TArray<AWorkArea*> AResourceGameMode::GetAllResourcePlaces(AWorkingUnitBase* Worker)
{
	// Clean up all arrays in WorkAreaGroups to remove invalid pointers
	WorkAreaGroups.PrimaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.SecondaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.TertiaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.RareAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.EpicAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });
	WorkAreaGroups.LegendaryAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area); });

	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.PrimaryAreas);
	AllAreas.Append(WorkAreaGroups.SecondaryAreas);
	AllAreas.Append(WorkAreaGroups.TertiaryAreas);
	AllAreas.Append(WorkAreaGroups.RareAreas);
	AllAreas.Append(WorkAreaGroups.EpicAreas);
	AllAreas.Append(WorkAreaGroups.LegendaryAreas);

	// Skip depleted resource areas (AvailableResourceAmount <= 0) and any stale/invalid pointers so
	// workers are never assigned to a place they cannot actually extract from.
	AllAreas.RemoveAll([](AWorkArea* Area) { return !IsValid(Area) || Area->AvailableResourceAmount <= 0.f; });

	// Drop everything this worker may not mine / may not hand in anywhere. Doing it here (and in
	// GetFiveClosestResourcePlaces) covers every automatic ResourcePlace assignment: this list
	// feeds AWorkArea::SwitchResourceArea, ABuildingBase::SwitchResourceArea and
	// GetNearestAvailableResourceOfTypeWithin.
	FilterResourcePlacesForWorker(Worker, AllAreas);

	// Sort all areas by distance to the worker's base (if available), otherwise use worker location
	const FVector ReferenceLocation = (Worker->Base && IsValid(Worker->Base))
		? Worker->Base->GetActorLocation()
		: Worker->GetActorLocation();

	AllAreas.Sort([ReferenceLocation](const AWorkArea& AreaA, const AWorkArea& AreaB) {
		return (AreaA.GetActorLocation() - ReferenceLocation).SizeSquared() <
			   (AreaB.GetActorLocation() - ReferenceLocation).SizeSquared();
	});

	return AllAreas;
}

EResourceType AResourceGameMode::GetWorkerRoutingResourceType(AWorkingUnitBase* Worker) const
{
	// Blueprint-facing wrapper; the logic lives on the worker so the controllers can use it
	// without reaching for the GameMode.
	return IsValid(Worker) ? Worker->GetRoutingResourceType() : EResourceType::MAX;
}

bool AResourceGameMode::CanTeamDeliverResourceType(int32 TeamId, EResourceType ResourceType) const
{
	if (ResourceType == EResourceType::MAX)
	{
		return true;
	}

	bool bFoundAnyBase = false;
	for (ABuildingBase* Base : WorkAreaGroups.BaseAreas)
	{
		if (!IsValid(Base) || Base->TeamId != TeamId || Base->GetUnitState() == UnitData::Dead)
		{
			continue;
		}

		bFoundAnyBase = true;
		if (Base->AcceptsResourceType(ResourceType))
		{
			return true;
		}
	}

	// No base of this team (yet) -> do not block gathering. Bases can still be built later, and
	// this keeps teams that never register a base behaving exactly as before.
	return !bFoundAnyBase;
}

void AResourceGameMode::FilterResourcePlacesForWorker(AWorkingUnitBase* Worker, TArray<AWorkArea*>& InOutAreas) const
{
	if (!Worker)
	{
		return;
	}

	const int32 TeamId = Worker->TeamId;
	InOutAreas.RemoveAll([this, Worker, TeamId](const AWorkArea* Area)
	{
		if (!IsValid(Area))
		{
			return true;
		}

		// 1) Is this worker allowed to gather that resource at all?
		if (!Worker->CanMineWorkArea(Area))
		{
			return true;
		}

		// 2) Could the load ever be handed in? Gathering something no base of the team accepts
		//    would strand the worker in an endless mine -> walk -> rejected loop.
		return !CanTeamDeliverResourceType(TeamId, ConvertToResourceType(Area->Type));
	});
}

AWorkArea* AResourceGameMode::GetNearestAvailableResourceOfTypeWithin(AWorkingUnitBase* Worker, TEnumAsByte<WorkAreaData::WorkAreaType> Type, float Radius)
{
	if (!Worker) return nullptr;

	const FVector WorkerLoc = Worker->GetActorLocation();
	AWorkArea* Best = nullptr;
	float BestDistSq = Radius * Radius; // only consider nodes within Radius

	// GetAllResourcePlaces already removes invalid + depleted nodes (sorted, but we re-check distance).
	for (AWorkArea* WA : GetAllResourcePlaces(Worker))
	{
		if (!IsValid(WA) || WA == Worker->ResourcePlace) continue;
		if (WA->Type != Type) continue;
		// Full node? (MaxWorkerCount <= 0 == unlimited)
		if (WA->MaxWorkerCount > 0 && WA->Workers.Num() >= WA->MaxWorkerCount) continue;

		const float DistSq = FVector::DistSquared(WorkerLoc, WA->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = WA;
		}
	}
	return Best;
}

AWorkArea* AResourceGameMode::GetSuitableWorkAreaToWorker(int TeamId, const TArray<AWorkArea*>& WorkAreas)
{
	AWorkArea* BestWorkArea = nullptr;
	const bool bDistributionSet = IsWorkerDistributionSet(TeamId);

	// Check if there is space for the worker in any WorkArea based on ResourceType
	for (AWorkArea* WorkArea : WorkAreas)
	{
		if (WorkArea)
		{
			// Per-NODE capacity is unconditional. It used to be nested inside the per-TYPE quota check
			// below, which made it dead code in the default setup: with no worker distribution
			// configured GetMaxWorkersForResourceType returns 0, so "CurrentWorkers < MaxWorkers" is
			// "n < 0" and never passes - the whole capacity-respecting branch was unreachable and every
			// caller fell through to the uncapped fallback underneath.
			if (!AWorkArea::HasFreeMiningSlotFor(WorkArea, nullptr))
			{
				continue;
			}

			// Per-TYPE quota only applies when the team actually configured a distribution.
			if (bDistributionSet)
			{
				const EResourceType ResourceType = ConvertToResourceType(WorkArea->Type);
				const int32 CurrentWorkers = GetCurrentWorkersForResourceType(TeamId, ResourceType);
				const int32 MaxWorkers = GetMaxWorkersForResourceType(TeamId, ResourceType);

				if (CurrentWorkers >= MaxWorkers)
				{
					continue;
				}

				// Distribution set -> prioritize distance. WorkAreas is already sorted by distance,
				// so the first qualifying one wins.
				return WorkArea;
			}

			// Ohne eingestellte Verteilung entscheidet die ENTFERNUNG, nicht die Auslastung.
			//
			// Hier stand vorher "nimm das Feld mit den wenigsten Arbeitern". WorkAreas ist nach
			// Entfernung zur Basis sortiert, diese Wahl hat die Sortierung aber komplett ignoriert:
			// ein leeres Feld am anderen Ende der Karte schlug ein halbvolles direkt vor der Tuer,
			// und die Arbeiter liefen dauernd quer ueber die Karte. Volle Felder fallen schon oben
			// durch HasFreeMiningSlotFor heraus - der erste Treffer ist also das naechste FREIE
			// Feld, und weiter weg wird nur gelaufen, wenn in der Naehe wirklich nichts frei ist.
			return WorkArea;
		}
	}

	// Fallback: if the per-TYPE quota blocked everything, relax that quota - but NOT the per-node cap.
	// Returning a full node here is what sent workers to a 3/3 deposit where they then stalled; with
	// this returning null the caller idles the worker instead and retries once a slot frees up.
	if (!BestWorkArea && bDistributionSet)
	{
		for (AWorkArea* WorkArea : WorkAreas)
		{
			if (WorkArea && AWorkArea::HasFreeMiningSlotFor(WorkArea, nullptr))
			{
				// Naechstes freies Feld, die Liste ist nach Entfernung sortiert.
				BestWorkArea = WorkArea;
				break;
			}
		}
	}
	
	return BestWorkArea;
}

void AResourceGameMode::AddMaxWorkersForResourceType(int TeamId, EResourceType ResourceType, float Amount)
{
	TArray<AActor*> TempActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorkingUnitBase::StaticClass(), TempActors);
	
	// Correctly use GetWorld()->GetAllActorsOfClass
	int32 TeamWorkerCount = 0;
	for (AActor* MyActor : TempActors)
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(MyActor);
		if (Worker && Worker->IsWorker && Worker->TeamId == TeamId)
		{
			{
				TeamWorkerCount++;
			}
		}
	}

	const int CurrentMaxWorkerCount = GetMaxWorkersForResourceType(TeamId, EResourceType::Primary) +
										GetMaxWorkersForResourceType(TeamId, EResourceType::Secondary) +
											GetMaxWorkersForResourceType(TeamId, EResourceType::Tertiary) +
												GetMaxWorkersForResourceType(TeamId, EResourceType::Rare) +
													GetMaxWorkersForResourceType(TeamId, EResourceType::Epic) +
														GetMaxWorkersForResourceType(TeamId, EResourceType::Legendary);
	
	// Check if the total worker count matches and amount is positive
	if ((CurrentMaxWorkerCount >= TeamWorkerCount && Amount >= 0) || (GetMaxWorkersForResourceType(TeamId, ResourceType) == 0 && Amount <= 0))
	{
		return; // Exit without modifying the worker count
	}
	
	for (FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (TeamId >= 0 && TeamId < ResourceArray.MaxWorkers.Num())
			{
				ResourceArray.MaxWorkers[TeamId] += Amount;
				break; // Exit once the correct resource type is modified
			}
		}
	}

	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources);
	}
}

void AResourceGameMode::SetCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType, float Amount)
{

	if(GetCurrentWorkersForResourceType(TeamId, ResourceType) == 0 && Amount <= 0) return;
	
	for (FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (TeamId >= 0 && TeamId < ResourceArray.CurrentWorkers.Num())
			{
				ResourceArray.CurrentWorkers[TeamId] = Amount;
				break; // Exit once the correct resource type is modified
			}
		}
	}

	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources);
	}
}

void AResourceGameMode::SetAllCurrentWorkers(int TeamId)
{
	TArray<AActor*> TempActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorkingUnitBase::StaticClass(), TempActors);
	
	TMap<EResourceType, int32> WorkerCountPerType;

	// Seed every type with 0. Without this the loop below only writes types that still have workers, so a
	// type whose last worker died or went off to build kept its old number and the HUD showed a phantom
	// "1/3" for the rest of the match.
	for (int32 TypeIndex = 0; TypeIndex < static_cast<int32>(EResourceType::MAX); ++TypeIndex)
	{
		WorkerCountPerType.Add(static_cast<EResourceType>(TypeIndex), 0);
	}

	for (AActor* MyActor : TempActors)
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(MyActor);
		if (Worker && Worker->ResourcePlace && Worker->TeamId == TeamId)
		{
			// Cast the Controller property to AWorkingUnitController
				EResourceType ResourceType = ConvertToResourceType(Worker->ResourcePlace->Type);
				// SAVE WORKERCOUNT DEPENDING ON RESOURCETYPE
				WorkerCountPerType.FindOrAdd(ResourceType)++;
		}
	}
	// Setting current workers for each resource type
	for (const auto& Pair : WorkerCountPerType)
	{
		SetCurrentWorkersForResourceType(TeamId, Pair.Key, Pair.Value);
	}
}

void AResourceGameMode::AddCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType, float Amount)
{

	if(GetCurrentWorkersForResourceType(TeamId, ResourceType) == 0 && Amount <= 0) return;
	
	for (FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (TeamId >= 0 && TeamId < ResourceArray.CurrentWorkers.Num())
			{
				ResourceArray.CurrentWorkers[TeamId] += Amount;
				break; // Exit once the correct resource type is modified
			}
		}
	}

	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources);
	}
}


int32 AResourceGameMode::GetCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType) const
{
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.CurrentWorkers.IsValidIndex(TeamId) && ResourceArray.ResourceType == ResourceType)
		{
			return ResourceArray.CurrentWorkers[TeamId];
		}
	}
	return 0;
}


int32 AResourceGameMode::GetMaxWorkersForResourceType(int TeamId, EResourceType ResourceType) const
{
	// Assuming 'AttributeSet' is correctly instantiated and holds the current workers' information.
	// Make sure 'AttributeSet' is accessible in this context. It might be a member of this class or accessed through another object.
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.MaxWorkers.IsValidIndex(TeamId) && ResourceArray.ResourceType == ResourceType)
		{
			return ResourceArray.MaxWorkers[TeamId];
		}
	}
	return 0;
}

bool AResourceGameMode::IsWorkerDistributionSet(int TeamId) const
{
	// Check if any MaxWorkers value is greater than 0 for the given team
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.MaxWorkers.IsValidIndex(TeamId) && ResourceArray.MaxWorkers[TeamId] > 0)
		{
			return true;
		}
	}
	return false;
}
