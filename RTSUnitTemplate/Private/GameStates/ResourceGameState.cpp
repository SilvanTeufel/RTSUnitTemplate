// Fill out your copyright notice in the Description page of Project Settings.


#include "GameStates/ResourceGameState.h"
#include "Net/UnrealNetwork.h"

void AResourceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AResourceGameState, TeamResources);
}

void AResourceGameState::OnRep_TeamResources()
{
	// Handle any logic needed when TeamResources updates, such as notifying UI elements to refresh.
}

void AResourceGameState::SetTeamResources(TArray<FResourceArray> Resources)
{
	TeamResources = Resources;

	/*
	// 1. We must get the Enum object here to convert the enum value to a string.
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EResourceType"), true);
	if (!EnumPtr)
	{
		// Safety check in case the enum can't be found.
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("--- GameState Updated: Displaying All Team Resources ---"));

	// 2. Loop through each resource type (e.g., Primary, Secondary).
	for (const FResourceArray& Data : TeamResources)
	{
		FString ResourceName = EnumPtr->GetValueAsString(Data.ResourceType);

		// 3. To get the TeamId, we loop through the 'Resources' array inside the struct.
		// The index of this array corresponds to the TeamId.
		for (int32 TeamIndex = 0; TeamIndex < Data.Resources.Num(); ++TeamIndex)
		{
			float Amount = Data.Resources[TeamIndex];
			UE_LOG(LogTemp, Warning, TEXT("   Team %d -> %s = %.1f"), TeamIndex, *ResourceName, Amount);
		}
	}
	// --- End of Debugging Log ---
	*/
}
