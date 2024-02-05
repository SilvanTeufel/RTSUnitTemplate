// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/ResourceGameMode.h"

AResourceGameMode::AResourceGameMode()
{
	// Initialize the resources for a predetermined number of teams/resource types
	InitializeResources(NumberOfTeams); // Specify the number of teams
}

void AResourceGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Initialize resources for the game
	InitializeResources(NumberOfTeams); 
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

// Adjusting the ModifyResource function to use the ResourceType within FResourceArray
void AResourceGameMode::ModifyResource(EResourceType ResourceType, int32 TeamId, float Amount)
{
	for (FResourceArray& ResourceArray : TeamResources)
	{
		if (ResourceArray.ResourceType == ResourceType)
		{
			if (TeamId >= 0 && TeamId < ResourceArray.Resources.Num())
			{
				ResourceArray.Resources[TeamId] += Amount;

				UE_LOG(LogTemp, Warning, TEXT("New Resource Amount: %f"), ResourceArray.Resources[TeamId]);
				break; // Exit once the correct resource type is modified
			}
		}
	}
}