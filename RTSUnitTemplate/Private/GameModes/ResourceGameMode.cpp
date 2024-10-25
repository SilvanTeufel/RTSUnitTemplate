// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/ResourceGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h" // For TActorIterator
#include "Actors/WorkArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "Controller/AIController/BuildingControllerBase.h"
#include "Controller/AIController/WorkerUnitControllerBase.h"
#include "GameStates/ResourceGameState.h"
#include "Net/UnrealNetwork.h"


AResourceGameMode::AResourceGameMode()
{
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

	for(int32 ResourceTypeIndex = 0; ResourceTypeIndex < static_cast<int32>(EResourceType::MAX); ++ResourceTypeIndex)
	{
		EResourceType ResourceType = static_cast<EResourceType>(ResourceTypeIndex);
		TeamResources.Add(FResourceArray(ResourceType, NumberOfTeams));
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
				ResourceArray.Resources[TeamId] += Amount;
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

		int32 ResourceAmount = ResourceArray.Resources[TeamId];

		// Check if the team has enough resources of the current type
		if (Costs.Contains(ResourceArray.ResourceType) && ResourceAmount < Costs[ResourceArray.ResourceType])
		{
			return false; // Not enough resources of this type
		}
	}

	// If all costs are affordable
	return true;
}

void AResourceGameMode::AssignWorkAreasToWorkers()
{
	TArray<AWorkingUnitBase*> AllWorkers;
	TArray<AActor*> TempActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorkingUnitBase::StaticClass(), TempActors);
	
	// Correctly use GetWorld()->GetAllActorsOfClass

	
	for (AActor* MyActor : TempActors)
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(MyActor);
		
		if (!Worker) continue;

		// Assign the closest base
		Worker->Base = GetClosestBaseFromArray(Worker, WorkAreaGroups.BaseAreas);

		// Assign one of the five closest resource places randomly
		TArray<AWorkArea*> WorkPlaces = GetFiveClosestResourcePlaces(Worker);
		AWorkArea* WorkPlace = GetRandomClosestWorkArea(WorkPlaces);
		//AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(WorkPlace->Type), +1.0f);
		Worker->ResourcePlace = WorkPlace;
		//SetAllCurrentWorkers(Worker->TeamId);
	}
}

void AResourceGameMode::AssignWorkAreasToWorker(AWorkingUnitBase* Worker)
{
	
	if (!Worker) return;

	// Assign the closest base
	Worker->Base = GetClosestBaseFromArray(Worker, WorkAreaGroups.BaseAreas);

	// Assign one of the five closest resource places randomly
	TArray<AWorkArea*> WorkPlaces = GetFiveClosestResourcePlaces(Worker);
	AWorkArea* WorkPlace = GetRandomClosestWorkArea(WorkPlaces);
	//AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(WorkPlace->Type), +1.0f);
	Worker->ResourcePlace = WorkPlace;
	//SetAllCurrentWorkers(Worker->TeamId);
}

ABuildingBase* AResourceGameMode::GetClosestBaseFromArray(AWorkingUnitBase* Worker, const TArray<ABuildingBase*>& Bases)
{
	ABuildingBase* ClosestBase = nullptr;
	float MinDistanceSquared = FLT_MAX;

	for (ABuildingBase* Base : Bases)
	{
		float DistanceSquared = (Base->GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
		if (DistanceSquared < MinDistanceSquared)
		{
			MinDistanceSquared = DistanceSquared;
			ClosestBase = Base;
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
	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.PrimaryAreas);
	AllAreas.Append(WorkAreaGroups.SecondaryAreas);
	AllAreas.Append(WorkAreaGroups.TertiaryAreas);
	AllAreas.Append(WorkAreaGroups.RareAreas);
	AllAreas.Append(WorkAreaGroups.EpicAreas);
	AllAreas.Append(WorkAreaGroups.LegendaryAreas);
	// Exclude BaseAreas and BuildAreas if they are not considered resource places

	// Sort all areas by distance to the worker
	AllAreas.Sort([Worker](const AWorkArea& AreaA, const AWorkArea& AreaB) {
		return (AreaA.GetActorLocation() - Worker->GetActorLocation()).SizeSquared() < 
			   (AreaB.GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
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
/*
TArray<AWorkArea*> AResourceGameMode::GetClosestBase(AWorkingUnitBase* Worker)
{
	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.BaseAreas);
	// Exclude BaseAreas and BuildAreas if they are not considered resource places

	// Sort all areas by distance to the worker
	AllAreas.Sort([Worker](const AWorkArea& AreaA, const AWorkArea& AreaB) {
		return (AreaA.GetActorLocation() - Worker->GetActorLocation()).SizeSquared() < 
			   (AreaB.GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
	});

	// Take up to the first five areas
	int32 NumAreas = FMath::Min(MaxResourceAreasToSet, AllAreas.Num());
	TArray<AWorkArea*> ClosestAreas;
	for (int i = 0; i < NumAreas; ++i)
	{
		ClosestAreas.Add(AllAreas[i]);
	}

	return ClosestAreas;
}*/

TArray<AWorkArea*> AResourceGameMode::GetClosestBuildPlaces(AWorkingUnitBase* Worker)
{
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
		if(!AllAreas[i]->PlannedBuilding)
		{
			ClosestAreas.Add(AllAreas[i]);
		}
	}

	AllAreas.Empty();

	return ClosestAreas;
}


float AResourceGameMode::GetResource(int TeamId, EResourceType RType)
{
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.Resources.IsValidIndex(TeamId) && ResourceArray.ResourceType == RType)
		{
			return ResourceArray.Resources[TeamId];
		}
	}
	return 0;
}

TArray<AWorkArea*> AResourceGameMode::GetClosestResourcePlaces(AWorkingUnitBase* Worker)
{
	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.PrimaryAreas);
	AllAreas.Append(WorkAreaGroups.SecondaryAreas);
	AllAreas.Append(WorkAreaGroups.TertiaryAreas);
	AllAreas.Append(WorkAreaGroups.RareAreas);
	AllAreas.Append(WorkAreaGroups.EpicAreas);
	AllAreas.Append(WorkAreaGroups.LegendaryAreas);
	// Exclude BaseAreas and BuildAreas if they are not considered resource places

	// Sort all areas by distance to the worker
	AllAreas.Sort([Worker](const AWorkArea& AreaA, const AWorkArea& AreaB) {
		return (AreaA.GetActorLocation() - Worker->GetActorLocation()).SizeSquared() < 
			   (AreaB.GetActorLocation() - Worker->GetActorLocation()).SizeSquared();
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

AWorkArea* AResourceGameMode::GetSuitableWorkAreaToWorker(int TeamId, const TArray<AWorkArea*>& WorkAreas)
{
	// Example structure for AllWorkers - replace with your actual collection
			TArray<AWorkArea*> SuitableWorkAreas;
	
			// Check if there is space for the worker in any WorkArea based on ResourceType
			for (AWorkArea* WorkArea : WorkAreas) // Replace with actual collection of work areas
			{
				if (WorkArea)
				{
						EResourceType ResourceType = ConvertToResourceType(WorkArea->Type); // Assume WorkArea has a ResourceType property
	
						int32 CurrentWorkers = GetCurrentWorkersForResourceType(TeamId, ResourceType);
						int32 MaxWorkers = GetMaxWorkersForResourceType(TeamId, ResourceType); // Implement this based on your AttributeSet

						if (CurrentWorkers < MaxWorkers)
						{
							SuitableWorkAreas.Add(WorkArea);
						}
				}
			}
	
			return GetRandomClosestWorkArea(SuitableWorkAreas);

	// If no suitable WorkArea found
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
		if (Worker && Worker->TeamId == TeamId)
		{
			// Cast the Controller property to AWorkingUnitController
			AWorkerUnitControllerBase* WorkerController = Cast<AWorkerUnitControllerBase>(Worker->GetController());
			if (WorkerController)
			{
				// This worker has a AWorkingUnitController, proceed as needed
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
	
	for (AActor* MyActor : TempActors)
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(MyActor);
		if (Worker && Worker->ResourcePlace && Worker->TeamId == TeamId)
		{
			// Cast the Controller property to AWorkingUnitController
			AWorkerUnitControllerBase* WorkerController = Cast<AWorkerUnitControllerBase>(Worker->GetController());
			if (WorkerController)
			{
				EResourceType ResourceType = ConvertToResourceType(Worker->ResourcePlace->Type);
				// SAVE WORKERCOUNT DEPENDING ON RESOURCETYPE
				WorkerCountPerType.FindOrAdd(ResourceType)++;
			}
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
