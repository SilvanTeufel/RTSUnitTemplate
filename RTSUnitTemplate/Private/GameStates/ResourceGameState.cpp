// Fill out your copyright notice in the Description page of Project Settings.


#include "GameStates/ResourceGameState.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/LoadingWidget.h"
#include "Controller/PlayerController/CameraControllerBase.h"

void AResourceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AResourceGameState, TeamResources);
	DOREPLIFETIME(AResourceGameState, IsSupplyLike);
	DOREPLIFETIME(AResourceGameState, LoadingWidgetConfig);
}

void AResourceGameState::OnRep_LoadingWidgetConfig()
{
	UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] AResourceGameState::OnRep_LoadingWidgetConfig: TriggerId=%d, StartTime=%f, Duration=%f"), 
		LoadingWidgetConfig.TriggerId, LoadingWidgetConfig.ServerWorldTimeStart, LoadingWidgetConfig.Duration);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(It->Get()))
		{
			if (CameraPC->IsLocalPlayerController())
			{
				UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] AResourceGameState::OnRep_LoadingWidgetConfig: Calling CheckForLoadingWidget on local PC %s"), *CameraPC->GetName());
				CameraPC->CheckForLoadingWidget();
			}
		}
	}
}

void AResourceGameState::OnRep_TeamResources()
{
	// Handle any logic needed when TeamResources updates, such as notifying UI elements to refresh.
}

void AResourceGameState::SetTeamResources(TArray<FResourceArray> Resources)
{
	TeamResources = Resources;
}
