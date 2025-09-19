#include "System/PlayerTeamSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"

void UPlayerTeamSubsystem::SetTeamForPlayer(APlayerController* PlayerController, int32 TeamId)
{
	if (PlayerController && PlayerController->PlayerState)
	{
		const int32 PlayerId = PlayerController->PlayerState->GetPlayerId();
		PendingTeams.FindOrAdd(PlayerId) = TeamId;
		UE_LOG(LogTemp, Log, TEXT("Team set for PlayerID %d -> %d"), PlayerId, TeamId);
	}
}

bool UPlayerTeamSubsystem::GetTeamForPlayer(APlayerController* PlayerController, int32& OutTeamId, bool bConsume)
{
	if (PlayerController && PlayerController->PlayerState)
	{
		const int32 PlayerId = PlayerController->PlayerState->GetPlayerId();
		if (int32* FoundTeam = PendingTeams.Find(PlayerId))
		{
			OutTeamId = *FoundTeam;
			if (bConsume)
			{
				PendingTeams.Remove(PlayerId);
			}
			return true;
		}
	}
	return false;
}

void UPlayerTeamSubsystem::ClearAll()
{
	PendingTeams.Empty();
}