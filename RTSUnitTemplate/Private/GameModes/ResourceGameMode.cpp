// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/ResourceGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h" // For TActorIterator
#include "Actors/WorkArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameStates/ResourceGameState.h"
#include "Hud/HUDBase.h"
#include "Controller/WorkerUnitControllerBase.h"
#include "Net/UnrealNetwork.h"


AResourceGameMode::AResourceGameMode()
{
	/*
	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources); 
	}*/
}

void AResourceGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitializeTeamResourcesAttributes();
	GatherWorkAreas();


}

void AResourceGameMode::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AResourceGameMode, TeamResources);
	DOREPLIFETIME(AResourceGameMode, NumberOfTeams);
}

void AResourceGameMode::InitializeTeamResourcesAttributes()
{
    for (int32 TeamIndex = 0; TeamIndex < NumberOfTeams; ++TeamIndex)
    {
        // Create and initialize a new instance of the UResourceAttributeSet for each team
        UResourceAttributeSet* NewAttributeSet = NewObject<UResourceAttributeSet>(this, UResourceAttributeSet::StaticClass());

    	// Initialize resource attributes to 0
    	NewAttributeSet->PrimaryResource.SetCurrentValue(0.0f);
    	NewAttributeSet->SecondaryResource.SetCurrentValue(0.0f);
    	NewAttributeSet->TertiaryResource.SetCurrentValue(0.0f);
    	NewAttributeSet->RareResource.SetCurrentValue(0.0f);
    	NewAttributeSet->EpicResource.SetCurrentValue(0.0f);
    	NewAttributeSet->LegendaryResource.SetCurrentValue(0.0f);

    	// Initialize worker attributes to 0
    	NewAttributeSet->PrimaryWorkers.SetBaseValue(1.0f);
    	NewAttributeSet->SecondaryWorkers.SetBaseValue(2.0f);
    	NewAttributeSet->TertiaryWorkers.SetBaseValue(0.0f);
    	NewAttributeSet->RareWorkers.SetBaseValue(0.0f);
    	NewAttributeSet->EpicWorkers.SetBaseValue(0.0f);
    	NewAttributeSet->LegendaryWorkers.SetBaseValue(0.0f);

    	NewAttributeSet->PrimaryWorkers.SetCurrentValue(0.0f);
    	NewAttributeSet->SecondaryWorkers.SetCurrentValue(0.0f);
    	NewAttributeSet->TertiaryWorkers.SetCurrentValue(0.0f);
    	NewAttributeSet->RareWorkers.SetCurrentValue(0.0f);
    	NewAttributeSet->EpicWorkers.SetCurrentValue(0.0f);
    	NewAttributeSet->LegendaryWorkers.SetCurrentValue(0.0f);
    	
    	TeamResourceAttributeSets.Add(NewAttributeSet);

    }
}

/*
void AResourceGameMode::InitializeTeamAbilitySystemComponents()
{
	for (int32 TeamIndex = 0; TeamIndex < NumberOfTeams; ++TeamIndex)
	{
		// Create an ASC for each team
		UAbilitySystemComponent* NewASC = NewObject<UAbilitySystemComponent>(this, UAbilitySystemComponent::StaticClass());
		NewASC->SetIsReplicated(true); // If your game is networked
		NewASC->InitAbilityActorInfo(this, this); // Initialize with the game mode as the owner and avatar
		TeamAbilitySystemComponents.Add(NewASC);
	}
}*/

void AResourceGameMode::ModifyTeamResourceAttributes(int32 TeamId, EResourceType ResourceType, float Amount)
{
    if (TeamResourceAttributeSets.IsValidIndex(TeamId))
    {
        UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
        if (AttributeSet)
        {
            //UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemComponent; // Obtain the ASC reference specific to your team/player

        	UE_LOG(LogTemp, Warning, TEXT("Modify Resource!"));
            // Check which resource type is being modified and apply the change
            switch (ResourceType)
            {
                case EResourceType::Primary:
                	AttributeSet->PrimaryResource.SetCurrentValue(AttributeSet->PrimaryResource.GetCurrentValue()+Amount);
                    break;
                case EResourceType::Secondary:
                	AttributeSet->SecondaryResource.SetCurrentValue(AttributeSet->SecondaryResource.GetCurrentValue()+Amount);
                    break;
                case EResourceType::Tertiary:
                	AttributeSet->TertiaryResource.SetCurrentValue(AttributeSet->TertiaryResource.GetCurrentValue()+Amount);
                    break;
                case EResourceType::Rare:
                	AttributeSet->RareResource.SetCurrentValue(AttributeSet->RareResource.GetCurrentValue()+Amount);
                    break;
                case EResourceType::Epic:
                	AttributeSet->EpicResource.SetCurrentValue(AttributeSet->EpicResource.GetCurrentValue()+Amount);
                    break;
                case EResourceType::Legendary:
                	AttributeSet->LegendaryResource.SetCurrentValue(AttributeSet->LegendaryResource.GetCurrentValue()+Amount);
                    break;
                default:
                    break;
            }
        }
    }
}

bool AResourceGameMode::CanAffordConstructionAttributes(const FBuildingCost& ConstructionCost, int32 TeamId) const
{
	if (TeamId < 0 || TeamId >= TeamResourceAttributeSets.Num())
		return false;

	const UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
	if (!AttributeSet)
		return false;

	// Check if the team has enough resources for each cost
	if (AttributeSet->PrimaryResource.GetCurrentValue() < ConstructionCost.PrimaryCost ||
		AttributeSet->SecondaryResource.GetCurrentValue() < ConstructionCost.SecondaryCost ||
		AttributeSet->TertiaryResource.GetCurrentValue() < ConstructionCost.TertiaryCost ||
		AttributeSet->RareResource.GetCurrentValue() < ConstructionCost.RareCost ||
		AttributeSet->EpicResource.GetCurrentValue() < ConstructionCost.EpicCost ||
		AttributeSet->LegendaryResource.GetCurrentValue() < ConstructionCost.LegendaryCost)
	{
		return false;
	}

	return true;
}

float AResourceGameMode::GetResourceAttribute(int TeamId, EResourceType RType)
{
	if (TeamId < 0 || TeamId >= TeamResourceAttributeSets.Num())
		return 0.0f;

	UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
	if (!AttributeSet)
		return 0.0f;

	switch (RType)
	{
	case EResourceType::Primary:
		return AttributeSet->PrimaryResource.GetCurrentValue();
	case EResourceType::Secondary:
		return AttributeSet->SecondaryResource.GetCurrentValue();
	case EResourceType::Tertiary:
		return AttributeSet->TertiaryResource.GetCurrentValue();
	case EResourceType::Rare:
		return AttributeSet->RareResource.GetCurrentValue();
	case EResourceType::Epic:
		return AttributeSet->EpicResource.GetCurrentValue();
	case EResourceType::Legendary:
		return AttributeSet->LegendaryResource.GetCurrentValue();
	default:
		return 0.0f;
	}
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
		case WorkAreaData::Base:
			WorkAreaGroups.BaseAreas.Add(WorkArea);
			break;
		case WorkAreaData::BuildArea:
			WorkAreaGroups.BuildAreas.Add(WorkArea);
			break;
		default:
			// Handle any cases not explicitly covered
			break;
		}
	}
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

		AWorkerUnitControllerBase* WorkerController = Cast<AWorkerUnitControllerBase>(Worker->GetController());
		if (WorkerController)
		{
			// Assign the closest base
			Worker->Base = GetClosestWorkArea(Worker, WorkAreaGroups.BaseAreas);
		}
	}
}

void AResourceGameMode::AssignWorkAreasToWorker(AWorkingUnitBase* Worker)
{
	
	if (!Worker) return;

	// Assign the closest base
	Worker->Base = GetClosestWorkArea(Worker, WorkAreaGroups.BaseAreas);
	
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

AWorkArea* AResourceGameMode::GetRandomClosestWorkArea(const TArray<AWorkArea*>& WorkAreas)
{
	if (WorkAreas.Num() > 0)
	{
		int32 Index = FMath::RandRange(0, WorkAreas.Num() - 1);
		return WorkAreas[Index];
	}

	return nullptr;
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

void AResourceGameMode::SetMaxWorkersForResourceType(int TeamId, EResourceType ResourceType, float amount)
{
	// Assuming 'AttributeSet' is correctly instantiated and holds the current workers' information.
	// Make sure 'AttributeSet' is accessible in this context. It might be a member of this class or accessed through another object.
	UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
	
	if (!AttributeSet) // Check if AttributeSet is valid
	{
		UE_LOG(LogTemp, Warning, TEXT("AttributeSet is null in GetMaxWorkersForResourceType"));
		return;
	}
	
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

	// Count all workers in the AttributeSet from every resource
	const float AttributeSetWorkerCount = AttributeSet->PrimaryWorkers.GetBaseValue() +
									AttributeSet->SecondaryWorkers.GetBaseValue() +
									AttributeSet->TertiaryWorkers.GetBaseValue() +
									AttributeSet->RareWorkers.GetBaseValue() +
									AttributeSet->EpicWorkers.GetBaseValue() +
									AttributeSet->LegendaryWorkers.GetBaseValue();
	
	// Check if the total worker count matches and amount is positive
	if (AttributeSetWorkerCount >= TeamWorkerCount && amount > 0.f)
	{
		return; // Exit without modifying the worker count
	}
	

	switch (ResourceType)
	{
	case EResourceType::Primary:
		{
			float NewValue = AttributeSet->PrimaryWorkers.GetBaseValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->PrimaryWorkers.SetBaseValue(NewValue);
		}
		break;
	case EResourceType::Secondary:
		{
			float NewValue = AttributeSet->SecondaryWorkers.GetBaseValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->SecondaryWorkers.SetBaseValue(NewValue);
		}
		break;
	case EResourceType::Tertiary:
		{
			float NewValue = AttributeSet->TertiaryWorkers.GetBaseValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->TertiaryWorkers.SetBaseValue(NewValue);
		}
		break;
	case EResourceType::Rare:
		{
			float NewValue = AttributeSet->RareWorkers.GetBaseValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->RareWorkers.SetBaseValue(NewValue);
		}
		break;
	case EResourceType::Epic:
		{
			float NewValue = AttributeSet->EpicWorkers.GetBaseValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->EpicWorkers.SetBaseValue(NewValue);
		}
		break;
	case EResourceType::Legendary:
		{
			float NewValue = AttributeSet->LegendaryWorkers.GetBaseValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->LegendaryWorkers.SetBaseValue(NewValue);
		}
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unrecognized ResourceType in GetMaxWorkersForResourceType"));
		break;
	}
}

void AResourceGameMode::SetCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType, float amount)
{
	// Assuming 'AttributeSet' is correctly instantiated and holds the current workers' information.
	// Make sure 'AttributeSet' is accessible in this context. It might be a member of this class or accessed through another object.
	UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
	
	if (!AttributeSet) // Check if AttributeSet is valid
		{
		UE_LOG(LogTemp, Warning, TEXT("AttributeSet is null in GetMaxWorkersForResourceType"));
		return;
		}

	switch (ResourceType)
	{
	case EResourceType::Primary:
		{
			float NewValue = AttributeSet->PrimaryWorkers.GetCurrentValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->PrimaryWorkers.SetCurrentValue(NewValue);
		}
		break;
	case EResourceType::Secondary:
		{
			float NewValue = AttributeSet->SecondaryWorkers.GetCurrentValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->SecondaryWorkers.SetCurrentValue(NewValue);
		}
		break;
	case EResourceType::Tertiary:
		{
			float NewValue = AttributeSet->TertiaryWorkers.GetCurrentValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->TertiaryWorkers.SetCurrentValue(NewValue);
		}
		break;
	case EResourceType::Rare:
		{
			float NewValue = AttributeSet->RareWorkers.GetCurrentValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->RareWorkers.SetCurrentValue(NewValue);
		}
		break;
	case EResourceType::Epic:
		{
			float NewValue = AttributeSet->EpicWorkers.GetCurrentValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->EpicWorkers.SetCurrentValue(NewValue);
		}
		break;
	case EResourceType::Legendary:
		{
			float NewValue = AttributeSet->LegendaryWorkers.GetCurrentValue()+amount;
			if(NewValue >= 0.f)
			AttributeSet->LegendaryWorkers.SetCurrentValue(NewValue);
		}
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unrecognized ResourceType in GetMaxWorkersForResourceType"));
		break;
	}
}


int32 AResourceGameMode::GetCurrentWorkersForResourceType(int TeamId, EResourceType ResourceType) const
{
	// Assuming 'AttributeSet' is correctly instantiated and holds the current workers' information.
	// Make sure 'AttributeSet' is accessible in this context. It might be a member of this class or accessed through another object.
	UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
	
	if (!AttributeSet) // Check if AttributeSet is valid
		{
		UE_LOG(LogTemp, Warning, TEXT("AttributeSet is null in GetMaxWorkersForResourceType"));
		return 0;
		}

	switch (ResourceType)
	{
	case EResourceType::Primary:
		return static_cast<int32>(AttributeSet->PrimaryWorkers.GetCurrentValue());
	case EResourceType::Secondary:
		return static_cast<int32>(AttributeSet->SecondaryWorkers.GetCurrentValue());
	case EResourceType::Tertiary:
		return static_cast<int32>(AttributeSet->TertiaryWorkers.GetCurrentValue());
	case EResourceType::Rare:
		return static_cast<int32>(AttributeSet->RareWorkers.GetCurrentValue());
	case EResourceType::Epic:
		return static_cast<int32>(AttributeSet->EpicWorkers.GetCurrentValue());
	case EResourceType::Legendary:
		return static_cast<int32>(AttributeSet->LegendaryWorkers.GetCurrentValue());
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unrecognized ResourceType in GetMaxWorkersForResourceType"));
		return 0;
	}
}


int32 AResourceGameMode::GetMaxWorkersForResourceType(int TeamId, EResourceType ResourceType) const
{
	// Assuming 'AttributeSet' is correctly instantiated and holds the current workers' information.
	// Make sure 'AttributeSet' is accessible in this context. It might be a member of this class or accessed through another object.
	UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
	
	if (!AttributeSet) // Check if AttributeSet is valid
		{
		UE_LOG(LogTemp, Warning, TEXT("AttributeSet is null in GetMaxWorkersForResourceType"));
		return 0;
		}

	switch (ResourceType)
	{
	case EResourceType::Primary:
		return static_cast<int32>(AttributeSet->PrimaryWorkers.GetBaseValue());
	case EResourceType::Secondary:
		return static_cast<int32>(AttributeSet->SecondaryWorkers.GetBaseValue());
	case EResourceType::Tertiary:
		return static_cast<int32>(AttributeSet->TertiaryWorkers.GetBaseValue());
	case EResourceType::Rare:
		return static_cast<int32>(AttributeSet->RareWorkers.GetBaseValue());
	case EResourceType::Epic:
		return static_cast<int32>(AttributeSet->EpicWorkers.GetBaseValue());
	case EResourceType::Legendary:
		return static_cast<int32>(AttributeSet->LegendaryWorkers.GetBaseValue());
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unrecognized ResourceType in GetMaxWorkersForResourceType"));
		return 0;
	}
}


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
}

TArray<AWorkArea*> AResourceGameMode::GetClosestBuildPlaces(AWorkingUnitBase* Worker)
{
	TArray<AWorkArea*> AllAreas;
	// Combine all resource areas into a single array for simplicity
	AllAreas.Append(WorkAreaGroups.BuildAreas);
	// Exclude BaseAreas and BuildAreas if they are not considered resource places

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