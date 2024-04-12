// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/ResourceGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h" // For TActorIterator
#include "Actors/WorkArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameStates/ResourceGameState.h"
#include "Hud/HUDBase.h"
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
	// Initialize resources for the game
	//InitializeResources(NumberOfTeams);
	//InitializeTeamAbilitySystemComponents();
	InitializeTeamResourcesAttributes();
	GatherWorkAreas();

	/*
	AResourceGameState* RGState = GetGameState<AResourceGameState>();
	if (RGState)
	{
		RGState->SetTeamResources(TeamResources); 
	}*/
	//AssignWorkAreasToWorkers();
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
/*
void AResourceGameMode::ModifyTeamWorkerAttributes(int32 TeamId, EResourceType ResourceType, float Amount, UAbilitySystemComponent* AbilitySystemComponent)
{
    if (TeamResourceAttributeSets.IsValidIndex(TeamId))
    {
        UResourceAttributeSet* AttributeSet = TeamResourceAttributeSets[TeamId];
        if (AttributeSet)
        {
        	//UAbilitySystemComponent* AbilitySystemComponent = TeamAbilitySystemComponents[TeamId]; // Obtain the ASC reference specific to your team/player

        	// Check which resource type is being modified and apply the change
        	if(AbilitySystemComponent)
            switch (ResourceType)
            {
                case EResourceType::Primary:
                    AbilitySystemComponent->ApplyModToAttributeUnsafe(AttributeSet->GetPrimaryWorkersAttribute(), EGameplayModOp::Additive, Amount);
                    break;
                case EResourceType::Secondary:
                    AbilitySystemComponent->ApplyModToAttributeUnsafe(AttributeSet->GetSecondaryWorkersAttribute(), EGameplayModOp::Additive, Amount);
                    break;
                case EResourceType::Tertiary:
                    AbilitySystemComponent->ApplyModToAttributeUnsafe(AttributeSet->GetTertiaryWorkersAttribute(), EGameplayModOp::Additive, Amount);
                    break;
                case EResourceType::Rare:
                    AbilitySystemComponent->ApplyModToAttributeUnsafe(AttributeSet->GetRareWorkersAttribute(), EGameplayModOp::Additive, Amount);
                    break;
                case EResourceType::Epic:
                    AbilitySystemComponent->ApplyModToAttributeUnsafe(AttributeSet->GetEpicWorkersAttribute(), EGameplayModOp::Additive, Amount);
                    break;
                case EResourceType::Legendary:
                    AbilitySystemComponent->ApplyModToAttributeUnsafe(AttributeSet->GetLegendaryWorkersAttribute(), EGameplayModOp::Additive, Amount);
                    break;
                default:
                    break;
            }
        }
    }
}
*/
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

		// Assign the closest base
		Worker->Base = GetClosestWorkArea(Worker, WorkAreaGroups.BaseAreas);

		/*
		// Assign one of the five closest resource places randomly
		TArray<AWorkArea*> WorkPlaces = GetClosestResourcePlaces(Worker);
		UE_LOG(LogTemp, Warning, TEXT("BBBBB"));

		AWorkArea* NewResourcePlace = GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlaces);

		if(Worker->ResourcePlace && NewResourcePlace && Worker->ResourcePlace->Type != NewResourcePlace->Type)
		{
			SetCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
			SetCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Worker->ResourcePlace->Type), -1.0f);
			Worker->ResourcePlace = NewResourcePlace;
		}else if(!Worker->ResourcePlace && NewResourcePlace)
		{
			SetCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
			Worker->ResourcePlace = NewResourcePlace;
		}
		*/
		//Worker->ResourcePlace = Worker->ResourcePlace = GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlaces); //GetRandomClosestWorkArea(WorkPlaces);
	}
}

void AResourceGameMode::AssignWorkAreasToWorker(AWorkingUnitBase* Worker)
{
	
	if (!Worker) return;

	// Assign the closest base
	Worker->Base = GetClosestWorkArea(Worker, WorkAreaGroups.BaseAreas);

	/*
	// Assign one of the five closest resource places randomly
	TArray<AWorkArea*> WorkPlaces = GetClosestResourcePlaces(Worker);
	UE_LOG(LogTemp, Warning, TEXT("WorkPlaces.Num(): %d"), WorkPlaces.Num());
	
	AWorkArea* NewResourcePlace = GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlaces);
	UE_LOG(LogTemp, Warning, TEXT("CCCC"));
	if(Worker->ResourcePlace && NewResourcePlace && Worker->ResourcePlace->Type != NewResourcePlace->Type)
	{
		UE_LOG(LogTemp, Warning, TEXT("SET!A"));
		SetCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		SetCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Worker->ResourcePlace->Type), -1.0f);
		Worker->ResourcePlace = NewResourcePlace;
	}else if(!Worker->ResourcePlace && NewResourcePlace)
	{
		UE_LOG(LogTemp, Warning, TEXT("SET!B"));
		SetCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		Worker->ResourcePlace = NewResourcePlace;
	}*/
	//UE_LOG(LogTemp, Warning, TEXT("CCCC"));
	//Worker->ResourcePlace = GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlaces); //GetRandomClosestWorkArea(WorkPlaces);
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
					//for (EResourceType ResourceType : TEnumRange<EResourceType>())
					//{
					
						EResourceType ResourceType = ConvertToResourceType(WorkArea->Type); // Assume WorkArea has a ResourceType property
						int32 CurrentWorkers = GetCurrentWorkersForResourceType(TeamId, ResourceType);
						int32 MaxWorkers = GetMaxWorkersForResourceType(TeamId, ResourceType); // Implement this based on your AttributeSet
					UE_LOG(LogTemp, Warning, TEXT("ResourceType: %d"), ResourceType);
					UE_LOG(LogTemp, Warning, TEXT("CurrentWorkers: %d"), CurrentWorkers);
					UE_LOG(LogTemp, Warning, TEXT("MaxWorkers: %d"), MaxWorkers);
					
						if (CurrentWorkers < MaxWorkers)
						{
							//UE_LOG(LogTemp, Warning, TEXT("ResourceType: %d"), ResourceType);
							//UE_LOG(LogTemp, Warning, TEXT("CurrentWorkers: %d"), CurrentWorkers);
							//UE_LOG(LogTemp, Warning, TEXT("MaxWorkers: %d"), MaxWorkers);
							SuitableWorkAreas.Add(WorkArea);
						}
					//}
				}
			}

			UE_LOG(LogTemp, Warning, TEXT("SuitableWorkAreas found: %d"), SuitableWorkAreas.Num());
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

/*

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

	if (TeamId < 0 || TeamId >= NumberOfTeams)
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
*/