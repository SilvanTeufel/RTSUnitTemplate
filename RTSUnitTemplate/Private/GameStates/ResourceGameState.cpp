// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


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
	DOREPLIFETIME(AResourceGameState, MatchStartTime);
	DOREPLIFETIME(AResourceGameState, bStartupFreezeReleased);
	DOREPLIFETIME(AResourceGameState, TeamUnitsLost);
}

void AResourceGameState::AddUnitLoss(int32 TeamId)
{
	if (!HasAuthority() || TeamId < 0)
	{
		return;
	}

	// Der Index IST die TeamId, deshalb bis dorthin auffuellen statt anzuhaengen - sonst
	// zeigt ein spaeter auftretendes Team auf den Zaehler eines anderen.
	if (TeamUnitsLost.Num() <= TeamId)
	{
		TeamUnitsLost.SetNumZeroed(TeamId + 1);
	}
	TeamUnitsLost[TeamId]++;

	// OnRep laeuft nur auf den Clients; auf dem Server (und im Einzelspieler) muss der
	// Aufruf von Hand kommen, sonst sieht der Host seine eigene Zahl nie.
	OnRep_TeamUnitsLost();
}

int32 AResourceGameState::GetKillsForTeam(int32 MyTeamId) const
{
	int32 Summe = 0;
	for (int32 i = 0; i < TeamUnitsLost.Num(); ++i)
	{
		if (i != MyTeamId)
		{
			Summe += TeamUnitsLost[i];
		}
	}
	return Summe;
}

int32 AResourceGameState::GetLossesForTeam(int32 MyTeamId) const
{
	return TeamUnitsLost.IsValidIndex(MyTeamId) ? TeamUnitsLost[MyTeamId] : 0;
}

void AResourceGameState::OnRep_TeamUnitsLost()
{
	// Die Zahl haengt am Team des lokalen Spielers - ohne ihn gibt es nichts zu melden
	// (dedizierter Server).
	UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = Welt->GetPlayerControllerIterator(); It; ++It)
	{
		ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		if (!PC || !PC->IsLocalPlayerController())
		{
			continue;
		}

		const int32 Jetzt = GetKillsForTeam(PC->SelectableTeamId);
		if (Jetzt != ZuletztGemeldeteAbschuesse)
		{
			ZuletztGemeldeteAbschuesse = Jetzt;
			OnKillCountChanged.Broadcast(Jetzt);
		}
		return;
	}
}

void AResourceGameState::OnRep_LoadingWidgetConfig()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(It->Get()))
		{
			if (CameraPC->IsLocalPlayerController())
			{
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
