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
}
