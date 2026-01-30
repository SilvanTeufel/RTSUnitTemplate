#include "Actors/WinLoseConfigActor.h"
#include "Net/UnrealNetwork.h"

AWinLoseConfigActor::AWinLoseConfigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AWinLoseConfigActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWinLoseConfigActor, WinConditions);
	DOREPLIFETIME(AWinLoseConfigActor, CurrentWinConditionIndex);
	DOREPLIFETIME(AWinLoseConfigActor, LoseCondition);
	DOREPLIFETIME(AWinLoseConfigActor, TeamId);
	DOREPLIFETIME(AWinLoseConfigActor, TargetResourceCount);
	DOREPLIFETIME(AWinLoseConfigActor, TargetGameTime);
	DOREPLIFETIME(AWinLoseConfigActor, WinLoseTargetTag);
	DOREPLIFETIME(AWinLoseConfigActor, WinConditionDisplayDuration);
	DOREPLIFETIME(AWinLoseConfigActor, GameStartDisplayDuration);
}

void AWinLoseConfigActor::BeginPlay()
{
	Super::BeginPlay();
}

void AWinLoseConfigActor::OnRep_CurrentWinConditionIndex()
{
	OnWinConditionChanged.Broadcast(this, GetCurrentWinCondition());
}

EWinLoseCondition AWinLoseConfigActor::GetCurrentWinCondition() const
{
	if (WinConditions.IsValidIndex(CurrentWinConditionIndex))
	{
		return WinConditions[CurrentWinConditionIndex];
	}
	return EWinLoseCondition::None;
}

bool AWinLoseConfigActor::IsLastWinCondition() const
{
	if (WinConditions.Num() == 0) return true;
	return CurrentWinConditionIndex >= WinConditions.Num() - 1;
}

void AWinLoseConfigActor::AdvanceToNextWinCondition()
{
	CurrentWinConditionIndex++;
	OnWinConditionChanged.Broadcast(this, GetCurrentWinCondition());
}
