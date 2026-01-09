// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/ResourceGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h" // For TActorIterator
#include "Actors/WorkArea.h"
#include "Actors/WinLoseConfigActor.h"
#include "Characters/Unit/BuildingBase.h"
#include "Controller/AIController/BuildingControllerBase.h"
#include "Controller/AIController/WorkerUnitControllerBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameStates/ResourceGameState.h"
#include "Net/UnrealNetwork.h"


#include "System/MapSwitchSubsystem.h"
#include "Engine/GameInstance.h"

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

	FTimerHandle WinLoseTimerHandle;
	GetWorldTimerManager().SetTimer(WinLoseTimerHandle, this, &AResourceGameMode::CheckWinLoseConditionTimer, 1.0f, true);
}

void AResourceGameMode::CheckWinLoseConditionTimer()
{
	CheckWinLoseCondition(nullptr);
}

void AResourceGameMode::CheckWinLoseCondition(AUnitBase* DestroyedUnit)
{
	if (!bInitialSpawnFinished || GetWorld()->GetTimeSeconds() < (float)GatherControllerTimer + 10.f + DelaySpawnTableTime) return;
	if (bWinLoseTriggered) return;
	Super::CheckWinLoseCondition(DestroyedUnit);
	if (bWinLoseTriggered) return;

	if (!WinLoseConfigActor || WinLoseConfigActor->WinLoseCondition != EWinLoseCondition::TeamReachedResourceCount) return;

	int32 TargetTeamId = WinLoseConfigActor->TeamId;
	const FBuildingCost& TargetResources = WinLoseConfigActor->TargetResourceCount;

	bool bReached = true;
	if (GetResource(TargetTeamId, EResourceType::Primary) < TargetResources.PrimaryCost) bReached = false;
	if (GetResource(TargetTeamId, EResourceType::Secondary) < TargetResources.SecondaryCost) bReached = false;
	if (GetResource(TargetTeamId, EResourceType::Tertiary) < TargetResources.TertiaryCost) bReached = false;
	if (GetResource(TargetTeamId, EResourceType::Rare) < TargetResources.RareCost) bReached = false;
	if (GetResource(TargetTeamId, EResourceType::Epic) < TargetResources.EpicCost) bReached = false;
	if (GetResource(TargetTeamId, EResourceType::Legendary) < TargetResources.LegendaryCost) bReached = false;

	if (bReached)
	{
		bWinLoseTriggered = true;
		FString TargetMapName = WinLoseConfigActor->WinLoseTargetMapName.ToSoftObjectPath().GetLongPackageName();

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMapSwitchSubsystem* MapSwitchSub = GI->GetSubsystem<UMapSwitchSubsystem>())
			{
				if (WinLoseConfigActor->DestinationSwitchTagToEnable != NAME_None && !TargetMapName.IsEmpty())
				{
					MapSwitchSub->MarkSwitchEnabledForMap(TargetMapName, WinLoseConfigActor->DestinationSwitchTagToEnable);
				}
			}
		}

		bool bAnyWon = false;
		bool bAnyLost = false;

		for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
			if (!PC) continue;

			int32 PlayerTeamId = PC->SelectableTeamId;

			bool bWon = (PlayerTeamId == TargetTeamId);
			if (bWon) bAnyWon = true; else bAnyLost = true;

			TWeakObjectPtr<ACameraControllerBase> WeakPC = PC;
			TSubclassOf<UWinLoseWidget> WidgetClass = WinLoseConfigActor->WinLoseWidgetClass;
			FName Tag = WinLoseConfigActor->DestinationSwitchTagToEnable;

			GetWorldTimerManager().SetTimer(PC->WinLoseTimerHandle, [WeakPC, bWon, WidgetClass, TargetMapName, Tag]()
			{
				if (ACameraControllerBase* StrongPC = WeakPC.Get())
				{
					StrongPC->Client_TriggerWinLoseUI(bWon, WidgetClass, TargetMapName, Tag);
				}
			}, bWon ? WinLoseConfigActor->WinDelay : WinLoseConfigActor->LoseDelay, false);
		}

		if (bAnyWon) WinLoseConfigActor->OnYouWonTheGame.Broadcast();
		if (bAnyLost) WinLoseConfigActor->OnYouLostTheGame.Broadcast();
	}
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
					ResourceArray.Resources[TeamId] -= Amount; // Inverted logic for Supply
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

		int32 ResourceAmount = ResourceArray.Resources[TeamId];

		// Check if the team has enough resources of the current type
		if (Costs.Contains(ResourceArray.ResourceType) && Costs[ResourceArray.ResourceType] > 0.f)
		{
			bool bIsSupply = SupplyLikeResources.Contains(ResourceArray.ResourceType) ? SupplyLikeResources[ResourceArray.ResourceType] : false;
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
	Worker->ResourcePlace = WorkPlace;

	if (Worker->ResourcePlace)
	{
		UE_LOG(LogTemp, Log, TEXT("Randomly assigned ResourcePlace: %s to Worker: %s"), *Worker->ResourcePlace->GetName(), *Worker->GetName());
	}
	else
	{
		// This case might occur if GetRandomClosestWorkArea can return nullptr
		UE_LOG(LogTemp, Warning, TEXT("Failed to select a random resource place for Worker: %s"), *Worker->GetName());
	}
}

ABuildingBase* AResourceGameMode::GetClosestBaseFromArray(AWorkingUnitBase* Worker, const TArray<ABuildingBase*>& Bases)
{
    ABuildingBase* ClosestBase = nullptr;
    float MinDistanceSquared = FLT_MAX;

    // The 'Bases' array is iterated over.
    for (ABuildingBase* Base : Bases)
    {
       // Here, 'Base' is checked with IsValid(), which is good practice.
       // BUT, the 'Worker' pointer is used immediately without any check.
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
		if(!AllAreas[i]->PlannedBuilding)
		{
			ClosestAreas.Add(AllAreas[i]);
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
				ResourceArray.Resources[TeamId] += CostMap[ResourceArray.ResourceType];
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
							//UE_LOG(LogTemp, Log, TEXT("Added Area!"));
							SuitableWorkAreas.Add(WorkArea);
						}
				}
			}

	
	return GetRandomClosestWorkArea(SuitableWorkAreas);
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
