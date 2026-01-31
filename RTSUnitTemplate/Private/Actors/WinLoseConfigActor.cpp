#include "Actors/WinLoseConfigActor.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

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
	DOREPLIFETIME(AWinLoseConfigActor, WinLoseTargetTags);
	DOREPLIFETIME(AWinLoseConfigActor, WinConditionDisplayDuration);
	DOREPLIFETIME(AWinLoseConfigActor, GameStartDisplayDuration);
	DOREPLIFETIME(AWinLoseConfigActor, InitialDisplayDelay);
	DOREPLIFETIME(AWinLoseConfigActor, TagProgress);
}

void AWinLoseConfigActor::BeginPlay()
{
	Super::BeginPlay();
}

void AWinLoseConfigActor::OnRep_CurrentWinConditionIndex()
{
	OnWinConditionChanged.Broadcast(this, GetCurrentWinCondition());
}

void AWinLoseConfigActor::OnRep_TagProgress()
{
	OnTagProgressUpdated.Broadcast(this);
}

EWinLoseCondition AWinLoseConfigActor::GetCurrentWinCondition() const
{
	if (WinConditions.IsValidIndex(CurrentWinConditionIndex))
	{
		return WinConditions[CurrentWinConditionIndex].Condition;
	}
	return EWinLoseCondition::None;
}

FWinConditionData AWinLoseConfigActor::GetCurrentWinConditionData() const
{
	if (WinConditions.IsValidIndex(CurrentWinConditionIndex))
	{
		return WinConditions[CurrentWinConditionIndex];
	}
	return FWinConditionData();
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

AWinLoseConfigActor* AWinLoseConfigActor::GetWinLoseConfigForTeam(const UObject* WorldContextObject, int32 MyTeamId)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;

	AWinLoseConfigActor* BestConfig = nullptr;
	AWinLoseConfigActor* GlobalConfig = nullptr;

	for (TActorIterator<AWinLoseConfigActor> It(World); It; ++It)
	{
		AWinLoseConfigActor* Config = *It;
		if (Config)
		{
			if (Config->TeamId == MyTeamId)
			{
				BestConfig = Config;
				break;
			}
			if (Config->TeamId == 0)
			{
				GlobalConfig = Config;
			}
		}
	}

	return BestConfig ? BestConfig : GlobalConfig;
}
