#include "System/PlayerTeamSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"

void UPlayerTeamSubsystem::SetTeamForPlayer(APlayerController* PlayerController, int32 TeamId)
{
	if (PlayerController && PlayerController->PlayerState)
	{
		// 1) PlayerId (kann sich über Travel ändern, aber wir speichern es trotzdem)
		const int32 PlayerId = PlayerController->PlayerState->GetPlayerId();
		PendingTeams.FindOrAdd(PlayerId) = TeamId;

		// 2) Spielername als stabilerer Schlüssel über Travel (sofern der Name gleich bleibt)
		const FString PlayerName = PlayerController->PlayerState->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			PendingTeamsByName.FindOrAdd(PlayerName) = TeamId;
		}

		// 3) Netzwerkadresse als letzter Fallback (Seamless behält Connection)
		const FString NetAddr = PlayerController->GetPlayerNetworkAddress();
		if (!NetAddr.IsEmpty())
		{
			PendingTeamsByNetworkAddress.FindOrAdd(NetAddr) = TeamId;
		}

		UE_LOG(LogTemp, Log, TEXT("Team set for Player (Id:%d, Name:%s, Addr:%s) -> %d"),
			PlayerId, *PlayerName, *NetAddr, TeamId);
	}
}

bool UPlayerTeamSubsystem::GetTeamForPlayer(APlayerController* PlayerController, int32& OutTeamId, bool bConsume)
{
	if (PlayerController && PlayerController->PlayerState)
	{
		const int32 PlayerId = PlayerController->PlayerState->GetPlayerId();
		const FString PlayerName = PlayerController->PlayerState->GetPlayerName();
		const FString NetAddr = PlayerController->GetPlayerNetworkAddress();

		// 1) Versuch per PlayerId
		if (int32* FoundTeam = PendingTeams.Find(PlayerId))
		{
			OutTeamId = *FoundTeam;
			if (bConsume)
			{
				PendingTeams.Remove(PlayerId);
				if (!PlayerName.IsEmpty()) { PendingTeamsByName.Remove(PlayerName); }
				if (!NetAddr.IsEmpty()) { PendingTeamsByNetworkAddress.Remove(NetAddr); }
			}
			return true;
		}

		// 2) Versuch per Spielername
		if (!PlayerName.IsEmpty())
		{
			if (int32* FoundTeamByName = PendingTeamsByName.Find(PlayerName))
			{
				OutTeamId = *FoundTeamByName;
				if (bConsume)
				{
					PendingTeamsByName.Remove(PlayerName);
					PendingTeams.Remove(PlayerId);
					if (!NetAddr.IsEmpty()) { PendingTeamsByNetworkAddress.Remove(NetAddr); }
				}
				return true;
			}
		}

		// 3) Versuch per Netzwerkadresse
		if (!NetAddr.IsEmpty())
		{
			if (int32* FoundTeamByAddr = PendingTeamsByNetworkAddress.Find(NetAddr))
			{
				OutTeamId = *FoundTeamByAddr;
				if (bConsume)
				{
					PendingTeamsByNetworkAddress.Remove(NetAddr);
					PendingTeams.Remove(PlayerId);
					if (!PlayerName.IsEmpty()) { PendingTeamsByName.Remove(PlayerName); }
				}
				return true;
			}
		}
	}
	return false;
}

void UPlayerTeamSubsystem::ClearAll()
{
	PendingTeams.Empty();
	PendingTeamsByName.Empty();
	PendingTeamsByNetworkAddress.Empty();
}