// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "GameModes/RTSGameModeBase.h"
#include "GameStates/ResourceGameState.h"
#include "Actors/WinLoseConfigActor.h"
#include "PlayerStart/PlayerStartBase.h"
#include "Characters/Camera/CameraBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Widgets/WinLoseWidget.h"
#include "Widgets/LoadingWidget.h"
#include "EngineUtils.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Hud/PathProviderHUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "AIController.h"
#include "MassReplicationFragments.h"
#include "MassSignalSubsystem.h"
#include "Actors/Waypoint.h"
#include "Characters/Unit/MassUnitBase.h"

#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "System/PlayerTeamSubsystem.h"
#include "Characters/Camera/BehaviorTree/RTSBTController.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "NavigationSystem.h"
#include "Actors/FogActor.h"
#include "NavMesh/NavMeshPath.h" // Für FPathFindingQuery, FNavPathPoint etc.
#include "NavFilters/NavigationQueryFilter.h" // Für UNavigationQueryFilter
#include "AI/Navigation/NavigationTypes.h" // Für FPathFindingResult

#include "Engine/Engine.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/States/ChaseStateProcessor.h"
#include "System/MapSwitchSubsystem.h"
#include "Engine/GameInstance.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"
#include "Mass/UnitMassTag.h"

// Defined further down; seeds the spawn-time StoredLocation anchor from the unit's waypoint.
static void SeedSpawnStoredLocationFromWaypoint(AUnitBase* UnitBase);



void ARTSGameModeBase::BeginPlay()
{
	// Ganz vorn, bevor irgendetwas den Zufall zieht - Super::BeginPlay() spawnt bereits.
	// Siehe Kommentar an FesterZufallsStartwert: ohne das laeuft jeder weitere PIE-Lauf
	// im selben Editor im Zufallsstrom des vorigen weiter.
	if (FesterZufallsStartwert != 0)
	{
		FMath::RandInit(FesterZufallsStartwert);
		FMath::SRandInit(FesterZufallsStartwert);
		UE_LOG(LogTemp, Warning, TEXT("[MessSeed] Partie startet mit festem Zufallsstartwert %d"), FesterZufallsStartwert);
	}

	Super::BeginPlay();

	TagsDestroyedCountMap.Empty();
	TagsAliveCountMap.Empty();
	TeamTagsDestroyedCountMap.Empty();
	TeamTagsAliveCountMap.Empty();
	bWinLoseTriggered = false;

	FillUnitArrays();

	FTimerHandle TimerHandleGatherController;
	GetWorldTimerManager().SetTimer(TimerHandleGatherController, this, &ARTSGameModeBase::SetTeamIdsAndWaypoints, GatherControllerTimer, false);

	// Show loading widget via GameState (robust for late joiners)
	if (LoadingWidgetClass)
	{
		if (AResourceGameState* GS = GetGameState<AResourceGameState>())
		{
			float WidgetDuration = FMath::Max(10.f, (float)GatherControllerTimer + 5.f + (AllUnits.Num() * LoadingTimePerUnit));
			
			if (WidgetDuration > 40.f)
				WidgetDuration = MaxLoadingTime;
			
			const int32 NewTriggerId = FMath::RandRange(1, 2147483647);

			GS->LoadingWidgetConfig.WidgetClass = LoadingWidgetClass;
			GS->LoadingWidgetConfig.Duration = WidgetDuration;
			GS->LoadingWidgetConfig.TriggerId = NewTriggerId;
			GS->LoadingWidgetConfig.ServerWorldTimeStart = GS->GetServerWorldTimeSeconds();
			GS->MatchStartTime = GS->LoadingWidgetConfig.ServerWorldTimeStart + WidgetDuration;
			
			// Trigger for local host immediately
			GS->OnRep_LoadingWidgetConfig();

			FTimerHandle ReleaseEffectAreasTimer;
			GetWorldTimerManager().SetTimer(ReleaseEffectAreasTimer, this, &ARTSGameModeBase::ReleaseEffectAreas, WidgetDuration, false);

			// Gleiche Dauer wie der Ladebildschirm: ab hier ist er weg und das Match ist sichtbar.
			FTimerHandle LoadingFinishedTimer;
			GetWorldTimerManager().SetTimer(LoadingFinishedTimer, this, &ARTSGameModeBase::HandleLoadingScreenFinished, WidgetDuration, false);
		}
		else
		{
		}
	}
	else
	{
	}

	FTimerHandle TimerHandleStartDataTable;
	GetWorldTimerManager().SetTimer(TimerHandleStartDataTable, this, &ARTSGameModeBase::DataTableTimerStart, GatherControllerTimer+5.f+DelaySpawnTableTime, false);

	GetWorldTimerManager().SetTimer(WinLoseTimerHandle, this, &ARTSGameModeBase::CheckWinLoseConditionTimer, 1.0f, true);
}

void ARTSGameModeBase::ReleaseEffectAreas()
{
	if (UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
		FMassEntityQuery ReleaseQuery(EntityManager.AsShared());
		ReleaseQuery.AddTagRequirement<FMassEffectAreaLoadingTag>(EMassFragmentPresence::All);

		TArray<FMassEntityHandle> EntitiesToRelease;
		FMassExecutionContext Context(EntityManager);
		ReleaseQuery.ForEachEntityChunk(Context, [&EntitiesToRelease](FMassExecutionContext& ChunkContext)
		{
			const int32 NumEntities = ChunkContext.GetNumEntities();
			for (int32 i = 0; i < NumEntities; ++i)
			{
				EntitiesToRelease.Add(ChunkContext.GetEntity(i));
			}
		});

		if (EntitiesToRelease.Num() > 0)
		{
			for (const FMassEntityHandle& Entity : EntitiesToRelease)
			{
				// FIX: Use Defer()
				EntityManager.Defer().RemoveTag<FMassEffectAreaLoadingTag>(Entity);
			}
		}
	}
}

void ARTSGameModeBase::InitializeWinLoseConfigActors()
{
	// Find all WinLoseConfigActors in the world
	WinLoseConfigActors.Empty();
	for (TActorIterator<AWinLoseConfigActor> It(GetWorld()); It; ++It)
	{
		AWinLoseConfigActor* Config = *It;
		if (Config)
		{
			WinLoseConfigActors.Add(Config);
			if (!WinLoseConfigActor)
			{
				WinLoseConfigActor = Config;
			}
		}
	}
}

void ARTSGameModeBase::CheckWinLoseConditionTimer()
{
	CheckWinLoseCondition(nullptr);
}

void ARTSGameModeBase::DataTableTimerStart()
{
	if(!DisableSpawn)SetupTimerFromDataTable_Implementation(FVector(0.f), nullptr);
	bInitialSpawnFinished = true;
}

void ARTSGameModeBase::TriggerWinLoseForPlayer(ACameraControllerBase* PC, bool bWon, AWinLoseConfigActor* Config)
{
	if (!PC || !Config) return;

	FString TargetMapName = Config->WinLoseTargetMapName.ToSoftObjectPath().GetLongPackageName();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMapSwitchSubsystem* MapSwitchSub = GI->GetSubsystem<UMapSwitchSubsystem>())
		{
			if (Config->DestinationSwitchTagToEnable != NAME_None && !TargetMapName.IsEmpty())
			{
				MapSwitchSub->MarkSwitchEnabledForMap(TargetMapName, Config->DestinationSwitchTagToEnable);
			}

			// A level can open more than one door - see AdditionalSwitchTagsToEnable.
			if (!TargetMapName.IsEmpty())
			{
				for (const FName& ExtraTag : Config->AdditionalSwitchTagsToEnable)
				{
					if (ExtraTag != NAME_None)
					{
						MapSwitchSub->MarkSwitchEnabledForMap(TargetMapName, ExtraTag);
					}
				}
			}
		}
	}

	TWeakObjectPtr<ACameraControllerBase> WeakPC = PC;
	TSubclassOf<UWinLoseWidget> WidgetClass = Config->WinLoseWidgetClass;
	FName Tag = Config->DestinationSwitchTagToEnable;

	GetWorldTimerManager().SetTimer(PC->WinLoseTimerHandle, [WeakPC, bWon, WidgetClass, TargetMapName, Tag]()
	{
		if (ACameraControllerBase* StrongPC = WeakPC.Get())
		{
			StrongPC->Client_TriggerWinLoseUI(bWon, WidgetClass, TargetMapName, Tag);
		}
	}, bWon ? Config->WinDelay : Config->LoseDelay, false);

	// On defeat, hand the player a read-only spectate camera. Base EnterSpectate is a no-op;
	// the AExodusGameMode override performs the pawn swap + vision setup.
	if (!bWon && PC)
	{
		EnterSpectate(PC, /*bRevealAll=*/false);
	}
}

void ARTSGameModeBase::EnterSpectate(AController* Controller, bool bRevealAll)
{
	// Intentional no-op base implementation; AExodusGameMode overrides this.
}

bool ARTSGameModeBase::IsAnyUnitWithTagAlive(const FGameplayTag& Tag, const TMap<FGameplayTag, int32>& AliveTagCounts) const
{
	return Tag.IsValid() && AliveTagCounts.Contains(Tag) && AliveTagCounts[Tag] > 0;
}

void ARTSGameModeBase::UpdateTagProgressForConfig(AWinLoseConfigActor* Config)
{
	if (!Config) return;

	FWinConditionData CurrentWinData = Config->GetCurrentWinConditionData();
	if (CurrentWinData.Condition == EWinLoseCondition::TaggedUnitsDestroyed || CurrentWinData.Condition == EWinLoseCondition::TaggedUnitsSpawned)
	{
		TArray<FTagProgress> NewTagProgress;
		const FGameplayTagContainer& TargetTags = CurrentWinData.WinLoseTargetTags.Num() > 0 ? CurrentWinData.WinLoseTargetTags : Config->WinLoseTargetTags;

		// Use a set to collect all tags we should track progress for
		TSet<FGameplayTag> AllTagsToTrack;
		for (auto TagIt = TargetTags.CreateConstIterator(); TagIt; ++TagIt)
		{
			AllTagsToTrack.Add(*TagIt);
		}
		for (const FGameplayTagCount& TagCount : CurrentWinData.TargetTagCounts)
		{
			AllTagsToTrack.Add(TagCount.Tag);
		}

		for (const FGameplayTag& TargetTag : AllTagsToTrack)
		{
			int32 AliveCount = 0;
			int32 DestroyedCount = 0;

			if (Config->TeamId != 0)
			{
				if (CurrentWinData.Condition == EWinLoseCondition::TaggedUnitsDestroyed)
				{
					for (auto& Pair : TeamTagsAliveCountMap)
					{
						if (Pair.Key != Config->TeamId)
						{
							AliveCount += Pair.Value.TagCounts.Contains(TargetTag) ? Pair.Value.TagCounts[TargetTag] : 0;
						}
					}
					for (auto& Pair : TeamTagsDestroyedCountMap)
					{
						if (Pair.Key != Config->TeamId)
						{
							DestroyedCount += Pair.Value.TagCounts.Contains(TargetTag) ? Pair.Value.TagCounts[TargetTag] : 0;
						}
					}
				}
				else // TaggedUnitsSpawned or other
				{
					if (TeamTagsAliveCountMap.Contains(Config->TeamId))
					{
						AliveCount = TeamTagsAliveCountMap[Config->TeamId].TagCounts.Contains(TargetTag) ? TeamTagsAliveCountMap[Config->TeamId].TagCounts[TargetTag] : 0;
					}
					if (TeamTagsDestroyedCountMap.Contains(Config->TeamId))
					{
						DestroyedCount = TeamTagsDestroyedCountMap[Config->TeamId].TagCounts.Contains(TargetTag) ? TeamTagsDestroyedCountMap[Config->TeamId].TagCounts[TargetTag] : 0;
					}
				}
			}
			else
			{
				AliveCount = TagsAliveCountMap.Contains(TargetTag) ? TagsAliveCountMap[TargetTag] : 0;
				DestroyedCount = TagsDestroyedCountMap.Contains(TargetTag) ? TagsDestroyedCountMap[TargetTag] : 0;
			}

			int32 TotalCount = AliveCount + DestroyedCount;

			FTagProgress Progress;
			Progress.Tag = TargetTag;
			Progress.AliveCount = AliveCount;
			Progress.TotalCount = TotalCount;

			// Determine TargetCount
			int32 TargetCount = 0;
			for (const FGameplayTagCount& TagCount : CurrentWinData.TargetTagCounts)
			{
				if (TagCount.Tag == TargetTag)
				{
					TargetCount = TagCount.Count;
					break;
				}
			}
			
			// Fallback to 1 if it's in the container but not in the count array
			if (TargetCount == 0 && TargetTags.HasTagExact(TargetTag))
			{
				TargetCount = 1;
			}

			Progress.TargetCount = TargetCount;
			NewTagProgress.Add(Progress);
		}
		Config->TagProgress = NewTagProgress;
	}
}

float ARTSGameModeBase::GetResource(int32 TeamId, EResourceType ResourceType) const
{
	return 0.f;
}

void ARTSGameModeBase::CheckWinLoseCondition(AUnitBase* DestroyedUnit)
{
	if (DestroyedUnit && bInitialSpawnFinished && !DestroyedUnit->DeadEffectsExecuted)
	{
		for (auto TagIt = DestroyedUnit->UnitTags.CreateConstIterator(); TagIt; ++TagIt)
		{
			TagsDestroyedCountMap.FindOrAdd(*TagIt)++;
			TeamTagsDestroyedCountMap.FindOrAdd(DestroyedUnit->TeamId).TagCounts.FindOrAdd(*TagIt)++;
		}
	}

	if (bWinLoseTriggered) return;
	
	// Prune invalid or destroyed config actors to avoid stale pointers
	WinLoseConfigActors.RemoveAllSwap([](AWinLoseConfigActor* C){ return C == nullptr || !IsValid(C); });
	
	if (WinLoseConfigActors.Num() == 0)
	{
		InitializeWinLoseConfigActors();
		if (WinLoseConfigActors.Num() == 0) return;
	}

	TArray<ABuildingBase*> AllBuildings;
	TagsAliveCountMap.Empty();
	TeamTagsAliveCountMap.Empty();
	bool bNeedBuildings = false;
	bool bNeedTags = false;

	for (AWinLoseConfigActor* Config : WinLoseConfigActors)
	{
		if (!IsValid(Config)) { continue; }
		EWinLoseCondition CurrentWinCondition = Config->GetCurrentWinCondition();
		if (CurrentWinCondition == EWinLoseCondition::AllBuildingsDestroyed || 
			Config->LoseCondition == EWinLoseCondition::AllBuildingsDestroyed)
		{
			bNeedBuildings = true;
		}
		
		if (CurrentWinCondition == EWinLoseCondition::TaggedUnitsDestroyed ||
			CurrentWinCondition == EWinLoseCondition::TaggedUnitsSpawned ||
			Config->LoseCondition == EWinLoseCondition::TaggedUnitsDestroyed || 
			Config->LoseCondition == EWinLoseCondition::TaggedUnitsSpawned ||
			Config->WinLoseTargetTags.Num() > 0)
		{
			bNeedTags = true;
		}

		for (const FWinConditionData& ConditionData : Config->WinConditions)
		{
			if (ConditionData.Condition == EWinLoseCondition::TaggedUnitsDestroyed || 
				ConditionData.Condition == EWinLoseCondition::TaggedUnitsSpawned ||
				ConditionData.WinLoseTargetTags.Num() > 0 ||
				ConditionData.TargetTagCounts.Num() > 0)
			{
				bNeedTags = true;
				break;
			}
		}
	}

	if (bNeedBuildings || bNeedTags)
	{
		for (TActorIterator<ASpawnerUnit> It(GetWorld()); It; ++It)
		{
			ASpawnerUnit* Spawner = *It;
			if (Spawner && Spawner != DestroyedUnit)
			{
				// Check if it's dead (if it has a state)
				if (AAbilityUnit* AbilityUnit = Cast<AAbilityUnit>(Spawner))
				{
					if (AbilityUnit->GetUnitState() == UnitData::Dead)
					{
						continue;
					}
				}

				if (ABuildingBase* Building = Cast<ABuildingBase>(Spawner))
				{
					AllBuildings.Add(Building);
				}
				
				for (auto TagIt = Spawner->UnitTags.CreateConstIterator(); TagIt; ++TagIt)
				{
					TagsAliveCountMap.FindOrAdd(*TagIt)++;
					TeamTagsAliveCountMap.FindOrAdd(Spawner->TeamId).TagCounts.FindOrAdd(*TagIt)++;
				}
			}
		}

		if (bNeedBuildings)
		{
			if (!bBuildingsEverExisted && AllBuildings.Num() > 0)
			{
				bBuildingsEverExisted = true;
			}
		}
	}

	// Update TagProgress for ALL configs so UI is correct even during delay
	for (AWinLoseConfigActor* Config : WinLoseConfigActors)
	{
		if (!IsValid(Config)) { continue; }
		UpdateTagProgressForConfig(Config);
	}

	if (!bInitialSpawnFinished) return;
	if (GetWorld()->GetTimeSeconds() < (float)GatherControllerTimer + 10.f + DelaySpawnTableTime) return;

	bool bAnyWon = false;
	bool bAnyLost = false;
	TArray<int32> LosingTeams;
	TSet<AWinLoseConfigActor*> AdvancedConfigs;

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
		if (!PC) continue;

		int32 PlayerTeamId = PC->SelectableTeamId;

		for (AWinLoseConfigActor* Config : WinLoseConfigActors)
		{
			if (!IsValid(Config)) continue;
			if (AdvancedConfigs.Contains(Config)) continue;
			if (Config->TeamId != 0 && Config->TeamId != PlayerTeamId) continue;

			bool bLocalTriggered = false;
			bool bWon = false;

			// 1. Check Win Condition
			bool bAdvancedInLoop = true;
			while (bAdvancedInLoop)
			{
				bAdvancedInLoop = false;
				FWinConditionData CurrentWinData = Config->GetCurrentWinConditionData();
				EWinLoseCondition CurrentWinCondition = CurrentWinData.Condition;

				bool bStepMet = false;
				bool bStepWon = false;

				if (CurrentWinCondition == EWinLoseCondition::AllBuildingsDestroyed)
				{
					bool bFriendlyBuildingsExist = false;
					bool bEnemyBuildingsExist = false;

					for (ABuildingBase* Building : AllBuildings)
					{
						if (Building->TeamId == PlayerTeamId) bFriendlyBuildingsExist = true;
						else bEnemyBuildingsExist = true;
					}

					if (!bEnemyBuildingsExist && bFriendlyBuildingsExist)
					{
						bStepMet = true;
						bStepWon = true;
					}
				}
				else if (CurrentWinCondition == EWinLoseCondition::TaggedUnitsDestroyed)
				{
					bool bAllTagsMet = true;
					bool bLastDestroyedWasFriendly = false;

					const FGameplayTagContainer& TargetTags = CurrentWinData.WinLoseTargetTags.Num() > 0 ? CurrentWinData.WinLoseTargetTags : Config->WinLoseTargetTags;
					if (TargetTags.Num() == 0 || Config->TagProgress.Num() == 0) bAllTagsMet = false;

					for (const FTagProgress& Progress : Config->TagProgress)
					{
						if (Progress.TotalCount > 0 && Progress.AliveCount == 0)
						{
							if (DestroyedUnit && DestroyedUnit->UnitTags.HasTagExact(Progress.Tag) && DestroyedUnit->TeamId == PlayerTeamId)
							{
								bLastDestroyedWasFriendly = true;
							}
						}
						else
						{
							bAllTagsMet = false;
						}
					}

					if (bAllTagsMet)
					{
						bStepMet = true;
						bStepWon = !bLastDestroyedWasFriendly;
					}
				}
				else if (CurrentWinCondition == EWinLoseCondition::TaggedUnitsSpawned)
				{
					bool bAllTagsMet = true;
					if (Config->TagProgress.Num() == 0) bAllTagsMet = false;

					for (const FTagProgress& Progress : Config->TagProgress)
					{
						if (Progress.TotalCount < Progress.TargetCount)
						{
							bAllTagsMet = false;
							break;
						}
					}

					if (bAllTagsMet)
					{
						bStepMet = true;
						bStepWon = true;
					}
				}
				else if (CurrentWinCondition == EWinLoseCondition::TeamReachedGameTime)
				{
					float CurrentTime = GetWorld()->GetTimeSeconds();
					if (AResourceGameState* GS = GetGameState<AResourceGameState>())
					{
						if (GS->MatchStartTime > 0)
						{
							CurrentTime = GS->GetServerWorldTimeSeconds() - GS->MatchStartTime;
						}
					}

					if (CurrentTime >= CurrentWinData.TargetGameTime)
					{
						bStepMet = true;
						bStepWon = (PlayerTeamId == Config->TeamId || Config->TeamId == 0);
					}
				}
				else if (CurrentWinCondition == EWinLoseCondition::TeamReachedResourceCount)
				{
					const FBuildingCost& Target = CurrentWinData.TargetResourceCount;
					if ((Target.PrimaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Primary) >= Target.PrimaryCost) &&
						(Target.SecondaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Secondary) >= Target.SecondaryCost) &&
						(Target.TertiaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Tertiary) >= Target.TertiaryCost) &&
						(Target.RareCost <= 0 || GetResource(PlayerTeamId, EResourceType::Rare) >= Target.RareCost) &&
						(Target.EpicCost <= 0 || GetResource(PlayerTeamId, EResourceType::Epic) >= Target.EpicCost) &&
						(Target.LegendaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Legendary) >= Target.LegendaryCost))
					{
						bStepMet = true;
						bStepWon = true;
					}
				}

				if (bStepMet && bStepWon)
				{
					if (!Config->IsLastWinCondition())
					{
						Config->AdvanceToNextWinCondition();
						AdvancedConfigs.Add(Config);
						UpdateTagProgressForConfig(Config); // Refresh data for the next step immediately
						bAdvancedInLoop = true;
					}
					else
					{
						bLocalTriggered = true;
						bWon = true;
						break;
					}
				}
			}

			// 2. Check Lose Condition (if Win Condition didn't trigger)
			if (!bLocalTriggered && Config->LoseCondition != EWinLoseCondition::None)
			{
				if (Config->LoseCondition == EWinLoseCondition::AllBuildingsDestroyed)
				{
					bool bTeamBuildingsExist = false;
					for (ABuildingBase* Building : AllBuildings)
					{
						if (Building->TeamId == PlayerTeamId)
						{
							bTeamBuildingsExist = true;
							break;
						}
					}

					if (!bTeamBuildingsExist)
					{
						bLocalTriggered = true;
						bWon = false;
					}
				}
				else if (Config->LoseCondition == EWinLoseCondition::TaggedUnitsDestroyed)
				{
					bool bAllTagsMet = true;
					if (Config->WinLoseTargetTags.Num() == 0) bAllTagsMet = false;

					for (auto TagIt = Config->WinLoseTargetTags.CreateConstIterator(); TagIt; ++TagIt)
					{
						const FGameplayTag& TargetTag = *TagIt;
						int32 AliveCount = 0;
						int32 DestroyedCount = 0;

						if (Config->TeamId != 0)
						{
							if (TeamTagsAliveCountMap.Contains(Config->TeamId))
							{
								AliveCount = TeamTagsAliveCountMap[Config->TeamId].TagCounts.Contains(TargetTag) ? TeamTagsAliveCountMap[Config->TeamId].TagCounts[TargetTag] : 0;
							}
							if (TeamTagsDestroyedCountMap.Contains(Config->TeamId))
							{
								DestroyedCount = TeamTagsDestroyedCountMap[Config->TeamId].TagCounts.Contains(TargetTag) ? TeamTagsDestroyedCountMap[Config->TeamId].TagCounts[TargetTag] : 0;
							}
						}
						else
						{
							AliveCount = TagsAliveCountMap.Contains(TargetTag) ? TagsAliveCountMap[TargetTag] : 0;
							DestroyedCount = TagsDestroyedCountMap.Contains(TargetTag) ? TagsDestroyedCountMap[TargetTag] : 0;
						}

						int32 TotalCount = AliveCount + DestroyedCount;

						if (!(TotalCount > 0 && AliveCount == 0))
						{
							bAllTagsMet = false;
							break;
						}
					}

					if (bAllTagsMet)
					{
						bLocalTriggered = true;
						bWon = false;
					}
				}
				else if (Config->LoseCondition == EWinLoseCondition::TeamReachedGameTime)
				{
					float CurrentTime = GetWorld()->GetTimeSeconds();
					if (AResourceGameState* GS = GetGameState<AResourceGameState>())
					{
						if (GS->MatchStartTime > 0)
						{
							CurrentTime = GS->GetServerWorldTimeSeconds() - GS->MatchStartTime;
						}
					}

					if (CurrentTime >= Config->TargetGameTime)
					{
						if (PlayerTeamId == Config->TeamId || Config->TeamId == 0)
						{
							bLocalTriggered = true;
							bWon = false;
						}
					}
				}
				else if (Config->LoseCondition == EWinLoseCondition::TeamReachedResourceCount)
				{
					const FBuildingCost& Target = Config->TargetResourceCount;
					if ((Target.PrimaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Primary) >= Target.PrimaryCost) &&
						(Target.SecondaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Secondary) >= Target.SecondaryCost) &&
						(Target.TertiaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Tertiary) >= Target.TertiaryCost) &&
						(Target.RareCost <= 0 || GetResource(PlayerTeamId, EResourceType::Rare) >= Target.RareCost) &&
						(Target.EpicCost <= 0 || GetResource(PlayerTeamId, EResourceType::Epic) >= Target.EpicCost) &&
						(Target.LegendaryCost <= 0 || GetResource(PlayerTeamId, EResourceType::Legendary) >= Target.LegendaryCost))
					{
						bLocalTriggered = true;
						bWon = false;
					}
				}
			}

			if (bLocalTriggered)
			{
				bWinLoseTriggered = true;
				if (bWon) bAnyWon = true; else { bAnyLost = true; LosingTeams.AddUnique(PlayerTeamId); }
				TriggerWinLoseForPlayer(PC, bWon, Config);
				break; // Found a trigger for this player
			}
		}
	}

	// Last team standing logic
	if (bAnyLost)
	{
		for (AWinLoseConfigActor* Config : WinLoseConfigActors)
		{
			if (!IsValid(Config)) continue;
			if (Config->TeamId == 0) // Global lose condition
			{
				TArray<int32> RemainingTeams;
				for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
				{
					ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
					if (PC && PC->SelectableTeamId != 0 && !LosingTeams.Contains(PC->SelectableTeamId))
					{
						RemainingTeams.AddUnique(PC->SelectableTeamId);
					}
				}

				if (RemainingTeams.Num() == 1)
				{
					int32 WinningTeamId = RemainingTeams[0];
					for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
					{
						ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
						if (PC && PC->SelectableTeamId == WinningTeamId && !PC->WinLoseTimerHandle.IsValid())
						{
							TriggerWinLoseForPlayer(PC, true, Config);
							bAnyWon = true;
						}
					}
				}
			}
		}
	}

	if (bAnyWon && IsValid(WinLoseConfigActor)) WinLoseConfigActor->OnYouWonTheGame.Broadcast();
	if (bAnyLost && IsValid(WinLoseConfigActor)) WinLoseConfigActor->OnYouLostTheGame.Broadcast();
}

void ARTSGameModeBase::Multicast_TriggerWinLoseUI_Implementation(bool bWon, TSubclassOf<class UWinLoseWidget> InWidgetClass, const FString& InMapName, FName DestinationSwitchTagToEnable)
{
}

void ARTSGameModeBase::NavInitialisation()
{
    UWorld* World = GetWorld();

	if (!World)
	{
		// This should almost never happen in a GameMode, but good to check
		UE_LOG(LogTemp, Error, TEXT("NavInitialisation: GetWorld() returned NULL!"));
		return;
	}
	
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
    if (NavSystem)
    {
        ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(); // Default NavData instance
        if (NavData)
        {
            // Try to find two random, reachable points on the NavMesh.
            FNavLocation PointA, PointB;
            bool bFoundPointA = NavSystem->GetRandomReachablePointInRadius(NavData->GetBounds().GetCenter(), 
                                                                            NavData->GetBounds().GetExtent().Size() / 2.0f, 
                                                                            PointA);
            bool bFoundPointB = NavSystem->GetRandomReachablePointInRadius(PointA.Location, 
                                                                            5000.0f, 
                                                                            PointB); // Second point near the first

            if (bFoundPointA && bFoundPointB && PointA.Location != PointB.Location)
            {
                UE_LOG(LogTemp, Log, TEXT("Found valid points. Performing dummy pathfinding query..."));

                // Use the controlled Pawn as the navigation agent - wenn es einen gibt.
                //
                // Auf einem Dedicated Server existiert zu diesem Zeitpunkt noch KEIN
                // PlayerController: es hat sich ja erst ein Client zu verbinden. Der
                // frueher hier stehende Direktzugriff
                //     World->GetFirstPlayerController()->GetPawn()
                // hat den Server deshalb beim Start mit EXCEPTION_ACCESS_VIOLATION
                // abgeschossen; die Pruefung auf !Pawn kam eine Zeile zu spaet.
                APlayerController* PC = World->GetFirstPlayerController();
                APawn* Pawn = PC ? PC->GetPawn() : nullptr;
                INavAgentInterface* NavAgent = Cast<INavAgentInterface>(Pawn);

            	// Create a default query filter
            	FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());

            	// The only pointer in this block that was never checked, and the warm-up query dereferences
            	// it. NavInitialisation crashed here with an access violation during level start; a missing
            	// filter is not worth taking the session down for, the query is only a warm-up.
            	if (!QueryFilter.IsValid())
            	{
            		UE_LOG(LogTemp, Warning, TEXT("NavInitialisation: no default nav query filter, skipping the warm-up query."));
            		return;
            	}

                // Ohne Nav-Agent (headless) wird die Aufwaermabfrage mit dieser GameMode-
                // Instanz als Owner gestellt. Das Ergebnis interessiert ohnehin nicht - der
                // einzige Zweck ist, das Navigationssystem einmal warmlaufen zu lassen -
                // und so passiert das auf dem Server genauso wie im Standalone-Spiel.
                FPathFindingQuery DummyQuery = NavAgent
                    ? FPathFindingQuery(*NavAgent, *NavData, PointA.Location, PointB.Location, QueryFilter)
                    : FPathFindingQuery(this, *NavData, PointA.Location, PointB.Location, QueryFilter);
                FPathFindingResult PathResult = NavSystem->FindPathSync(DummyQuery);
                UE_LOG(LogTemp, Log, TEXT("Dummy pathfinding complete. Success: %s"), 
                       PathResult.IsSuccessful() ? TEXT("Yes") : TEXT("No"));

            	if (PathResult.IsSuccessful())
            	{
            		PathfindingIsRdy = true;
            	}
            	
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Could not find two valid random points on the NavMesh for warmup query."));
                // Optional: try with fixed coordinates known to be valid.
            }
        }
        else
        {
        }
    }
    else
    {
    }
}


void ARTSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (NewPlayer)
	{
		SetupLoadingWidgetForPlayer(NewPlayer);
	}
}

void ARTSGameModeBase::HandleSeamlessTravelPlayer(AController*& C)
{
	ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(C);
	if (CameraPC && CameraPC->bIsAi)
	{
		// AI PlayerController should not travel to the new level
		UE_LOG(LogTemp, Log, TEXT("[GM] AI PlayerController %s detected during seamless travel. Destroying to prevent transition."), *C->GetName());
		
		if (APawn* Pawn = CameraPC->GetPawn())
		{
			Pawn->Destroy();
		}
		CameraPC->Destroy();
		C = nullptr;
	}
	else
	{
		Super::HandleSeamlessTravelPlayer(C);
	}
}

void ARTSGameModeBase::HandleLoadingScreenFinished()
{
	OnLoadingScreenFinished.Broadcast();
}

void ARTSGameModeBase::SetupLoadingWidgetForPlayer(APlayerController* NewPlayer)
{
	if (AResourceGameState* GS = GetGameState<AResourceGameState>())
	{
		if (GS->LoadingWidgetConfig.WidgetClass && GS->LoadingWidgetConfig.Duration > 0.f && GS->LoadingWidgetConfig.ServerWorldTimeStart >= 0.f)
		{
			if (ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(NewPlayer))
			{
				float CurrentServerTime = GS->GetServerWorldTimeSeconds();
				float Elapsed = CurrentServerTime - GS->LoadingWidgetConfig.ServerWorldTimeStart;
				float Remaining = GS->LoadingWidgetConfig.Duration - Elapsed;

				if (Remaining > 0.05f)
				{
					CameraPC->Client_ShowLoadingWidget(GS->LoadingWidgetConfig.WidgetClass, GS->LoadingWidgetConfig.Duration, GS->LoadingWidgetConfig.ServerWorldTimeStart, GS->LoadingWidgetConfig.TriggerId);
				}
			}
		}
	}
}

void ARTSGameModeBase::ApplyCustomizationsFromPlayerStart(APlayerController* PC, const APlayerStartBase* CustomStart)
{
	if (!PC || !CustomStart)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyCustomizationsFromPlayerStart: Invalid PC or CustomStart"));
		return;
	}

	// --- Update the PlayerController with sound settings ---
	ACameraControllerBase* CameraController = Cast<ACameraControllerBase>(PC);
	if (CameraController)
	{
		// Check each sound property in the CustomStart. If available, use it; otherwise, use the current controller setting.
		USoundBase* NewWaypointSound = CustomStart->WaypointSound ? CustomStart->WaypointSound : CameraController->WaypointSound;
		USoundBase* NewRunSound = CustomStart->RunSound ? CustomStart->RunSound : CameraController->RunSound;
		USoundBase* NewAbilitySound = CustomStart->AbilitySound ? CustomStart->AbilitySound : CameraController->AbilitySound;
		USoundBase* NewAttackSound = CustomStart->AttackSound ? CustomStart->AttackSound : CameraController->AttackSound;
		USoundBase* NewDropWorkAreaFailedSound = CustomStart->DropWorkAreaFailedSound ? CustomStart->DropWorkAreaFailedSound : CameraController->DropWorkAreaFailedSound;
		USoundBase* NewDropWorkAreaSound = CustomStart->DropWorkAreaSound ? CustomStart->DropWorkAreaSound : CameraController->DropWorkAreaSound;

		// Update the controller's properties on the server.
		CameraController->WaypointSound = NewWaypointSound;
		CameraController->RunSound = NewRunSound;
		CameraController->AbilitySound = NewAbilitySound;
		CameraController->AttackSound = NewAttackSound;
		CameraController->DropWorkAreaFailedSound = NewDropWorkAreaFailedSound;
		CameraController->DropWorkAreaSound = NewDropWorkAreaSound;

		// Now push the updated settings to the client via a client RPC.
		CameraController->Client_ApplyCustomizations(
			NewWaypointSound,
			NewRunSound,
			NewAbilitySound,
			NewAttackSound,
			NewDropWorkAreaFailedSound,
			NewDropWorkAreaSound);
        
		
		UE_LOG(LogTemp, Log, TEXT("Updated sounds on PlayerController: %s"), *CameraController->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerController is not of type ACameraControllerBase"));
	}
	// --- Update widget settings on the pawn ---
	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		AExtendedCameraBase* CameraPawn = Cast<AExtendedCameraBase>(Pawn);
		if (CameraPawn)
		{
			
			// Update the WidgetSelector widget component if a new widget class is provided.
			if (CustomStart->SelectorWidget)
			{
				CameraPawn->UnitSelectorWidget = CustomStart->SelectorWidget;
				UE_LOG(LogTemp, Log, TEXT("Updated WidgetSelector on Pawn: %s"), *CameraPawn->GetName());
			}
    
			// Update the TaggedSelector widget component.
			if (CustomStart->TaggedUnitSelector)
			{
				CameraPawn->TaggedSelectorWidget = CustomStart->TaggedUnitSelector;
				UE_LOG(LogTemp, Log, TEXT("Updated TaggedSelector on Pawn: %s"), *CameraPawn->GetName());
			}
    
			// Update the ResourceWidget widget component.
			if (CustomStart->ResourceWidget)
			{
				CameraPawn->ResourceWidget = CustomStart->ResourceWidget;
				UE_LOG(LogTemp, Log, TEXT("Updated ResourceWidget on Pawn: %s"), *CameraPawn->GetName());
			}

			// Call Client RPC to update widgets on the client
			CameraPawn->Client_UpdateWidgets(CustomStart->SelectorWidget, CustomStart->TaggedUnitSelector, CustomStart->ResourceWidget);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Pawn is not of type ACameraBase"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Pawn possessed by PlayerController: %s"), *PC->GetName());
	}
}


void ARTSGameModeBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(ARTSGameModeBase, SpawnTimerHandleMap);
	DOREPLIFETIME(ARTSGameModeBase, TimerIndex);
	DOREPLIFETIME(ARTSGameModeBase, AllUnits);
	DOREPLIFETIME(ARTSGameModeBase, CameraUnits);
}

int32 FindMatchingIndex(const TArray<int32>& IdArray, int32 SearchId)
{
	int32 Index = INDEX_NONE; // Initialize Index with an invalid value

	for (int32 i = 0; i < IdArray.Num(); ++i)
	{
		if (IdArray[i] == SearchId)
		{
			Index = i;
			break;
		}
	}

	return Index; // Return the found index, or INDEX_NONE if not found
}

void ARTSGameModeBase::SetTeamIdAndDefaultWaypoint_Implementation(int Id, AWaypoint* Waypoint, ACameraControllerBase* CameraControllerBase)
{

	if(CameraControllerBase)
	{
		CameraControllerBase->Multi_SetControllerTeamId(Id);
		CameraControllerBase->Multi_SetControllerDefaultWaypoint(Waypoint);
	}
	
}

void ARTSGameModeBase::FillUnitArrays()
{
	TArray <AActor*> GatheredUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnitBase::StaticClass(), GatheredUnits);
	
	for(int i = 0; i < GatheredUnits.Num(); i++)
	{
		HighestUnitIndex++;
	
		AUnitBase* Unit = Cast<AUnitBase>(GatheredUnits[i]);
		FGameplayTag CameraUnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Character.CameraUnit")));
		if (Unit)
		{
			Unit->SetUnitIndex(HighestUnitIndex);
			AllUnits.Add(Unit);

			if (Unit && Unit->UnitTags.HasTagExact(CameraUnitTag))
			{
				CameraUnits.Add(Unit);
			}
		}

		ASpeakingUnit* SpeakingUnit = Cast<ASpeakingUnit>(Unit);
		if(SpeakingUnit)
			SpeakingUnits.Add(SpeakingUnit);

		AWorkingUnitBase* WorkingUnit = Cast<AWorkingUnitBase>(Unit);
		if(WorkingUnit)
			WorkingUnits.Add(WorkingUnit);
	}
}

void ARTSGameModeBase::SetTeamIdsAndWaypoints_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("SetTeamIdsAndWaypoints_Implementation!!! World=%s HasAuthority=%s"), *GetNameSafe(GetWorld()), HasAuthority() ? TEXT("true") : TEXT("false"));
	
	// Gather all PlayerStarts in the level
	TArray<APlayerStartBase*> PlayerStarts;
	for (TActorIterator<APlayerStartBase> It(GetWorld()); It; ++It)
	{
		PlayerStarts.Add(*It);
	}
	UE_LOG(LogTemp, Log, TEXT("[GM] Found %d PlayerStarts. Listing..."), PlayerStarts.Num());
	for (int32 i = 0; i < PlayerStarts.Num(); ++i)
	{
		const APlayerStartBase* PS = PlayerStarts[i];
		UE_LOG(LogTemp, Log, TEXT("[GM] PlayerStart[%d]=%s TeamId=%d bIsAi=%s Waypoint=%s"), i, *GetNameSafe(PS), PS ? PS->SelectableTeamId : -1, (PS && PS->bIsAi) ? TEXT("true") : TEXT("false"), *GetNameSafe(PS ? PS->DefaultWaypoint : nullptr));
	}

	// Team assignment subsystem (server)
	UPlayerTeamSubsystem* TeamSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPlayerTeamSubsystem>() : nullptr;

	int32 PlayerIndex = 0;
	TSet<int32> OccupiedTeamIds; // teams that already have a controller assigned

	// First, assign existing player controllers (humans)
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* PlayerController = It->Get();
		ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(PlayerController);
		if (!CameraControllerBase)
		{
			continue;
		}

		if (CameraControllerBase->CameraBase)
		{
			CameraControllerBase->CameraBase->BlockControls = true;
		}

		// 1) Get TeamId from Lobby subsystem (if any)
		int32 LobbyTeamId = 0;
		if (TeamSubsystem && CameraControllerBase->PlayerState)
		{
			UE_LOG(LogTemp, Log, TEXT("TeamSubsystem FOUND!"));
			// bConsume=true: remove the entry after using it
			TeamSubsystem->GetTeamForPlayer(CameraControllerBase, LobbyTeamId, true);
			UE_LOG(LogTemp, Log, TEXT("LobbyTeamId: %d"), LobbyTeamId);
		}

		// 2) Find appropriate PlayerStart
		APlayerStartBase* CustomPlayerStart = nullptr;
		if (LobbyTeamId != 0)
		{
			for (APlayerStartBase* Start : PlayerStarts)
			{
				if (Start && Start->SelectableTeamId == LobbyTeamId && !Start->bIsAi)
				{
					CustomPlayerStart = Start; // prefer human-designated starts
					break;
				}
			}
			// If no non-AI start found for this team, allow any start with that team id
			if (!CustomPlayerStart)
			{
				for (APlayerStartBase* Start : PlayerStarts)
				{
					if (Start && Start->SelectableTeamId == LobbyTeamId)
					{
						CustomPlayerStart = Start;
						break;
					}
				}
			}
		}

		// Fallback: when no lobby selection present or not found
		if (!CustomPlayerStart && PlayerStarts.Num() > 0)
		{
			// If no lobby selection, prefer a non-AI PlayerStart for humans when available
			if (LobbyTeamId == 0)
			{
				for (APlayerStartBase* Start : PlayerStarts)
				{
					if (Start && !Start->bIsAi)
					{
						CustomPlayerStart = Start;
						break;
					}
				}
			}
			// If still none, fallback to index-based selection
			if (!CustomPlayerStart)
			{
				CustomPlayerStart = PlayerStarts[PlayerIndex % PlayerStarts.Num()];
				LobbyTeamId = CustomPlayerStart->SelectableTeamId; // adopt team id from the start
			}
		}

		if (!CustomPlayerStart)
		{
			UE_LOG(LogTemp, Warning, TEXT("No suitable PlayerStart found for controller %s"), *CameraControllerBase->GetName());
			if (CameraControllerBase->CameraBase)
			{
				CameraControllerBase->CameraBase->BlockControls = false;
			}
			continue;
		}

		// Ensure this PlayerStart isn't assigned to another controller in this pass
		PlayerStarts.RemoveSingle(CustomPlayerStart);
		// 3) Apply customizations and set team/waypoint
		ApplyCustomizationsFromPlayerStart(CameraControllerBase, CustomPlayerStart);

		const int32 FinalTeamId = CustomPlayerStart->SelectableTeamId;
		UE_LOG(LogTemp, Log, TEXT("Assigning TeamId: %d to Controller: %s"), FinalTeamId, *CameraControllerBase->GetName());

		SetTeamIdAndDefaultWaypoint_Implementation(FinalTeamId, CustomPlayerStart->DefaultWaypoint, CameraControllerBase);
		OccupiedTeamIds.Add(FinalTeamId);

		UE_LOG(LogTemp, Log, TEXT("TeamId is now: %d from Controller: %s"),
			CameraControllerBase->SelectableTeamId, *CameraControllerBase->GetName());
		UE_LOG(LogTemp, Log, TEXT("AllUnits.Num(): %d"), AllUnits.Num());

		// 4) Remaining initialization
		CameraControllerBase->Multi_SetMyTeamUnits(AllUnits);
		CameraControllerBase->Multi_SetCamLocation(CustomPlayerStart->GetActorLocation());

		// Find camera unit by tag
		FName SpecificCameraUnitTagName = FName(*FString::Printf(TEXT("Character.CameraUnit.%d"), PlayerIndex));
		FGameplayTag SpecificCameraUnitTag = FGameplayTag::RequestGameplayTag(SpecificCameraUnitTagName);
		CameraControllerBase->SetCameraUnitWithTag_Implementation(SpecificCameraUnitTag, CameraControllerBase->SelectableTeamId);
		CameraControllerBase->Multi_SetCameraOnly();
		CameraControllerBase->Client_InitializeMainHUD();
		
		CameraControllerBase->Multi_HideEnemyWaypoints();
		CameraControllerBase->Multi_InitFogOfWar();
		CameraControllerBase->Multi_SetupPlayerMiniMap();

		if (CameraControllerBase->CameraBase)
		{
			CameraControllerBase->CameraBase->BlockControls = false;
		}

		CameraControllerBase->AgentInit();

		PlayerIndex++;
	}

	// Now ensure AI players are spawned for AI-designated PlayerStarts whose teams have no controller
	for (APlayerStartBase* Start : PlayerStarts)
	{
		if (!Start || !Start->bIsAi)
		{
			continue;
		}

		const int32 TeamId = Start->SelectableTeamId;
		// Allow multiple PlayerControllers per TeamId and AI coexisting with humans/controllers.
		if (OccupiedTeamIds.Contains(TeamId))
		{
			UE_LOG(LogTemp, Log, TEXT("[GM] Team %d already has controllers, but multiple controllers per team are allowed. Proceeding to spawn AI."), TeamId);
		}

		UClass* PawnClass = AIPlayerPawnClass ? *AIPlayerPawnClass : ARLAgent::StaticClass();
		UE_LOG(LogTemp, Log, TEXT("[GM] AI spawn candidate at Start=%s Team=%d PawnClass=%s ControllerClass=%s"), *GetNameSafe(Start), TeamId, *GetNameSafe(AIPlayerPawnClass ? *AIPlayerPawnClass : nullptr), *GetNameSafe(AIPlayerControllerClass ? *AIPlayerControllerClass : nullptr));
		if (!PawnClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("AIPlayerPawnClass is null and default ARLAgent class not found. Skipping AI spawn."));
			continue;
		}

		// Spawn a PlayerController for the AI (configurable class)
		UClass* AIControllerClass = AIPlayerControllerClass ? *AIPlayerControllerClass : ACameraControllerBase::StaticClass();
		ACameraControllerBase* AIPC = GetWorld()->SpawnActor<ACameraControllerBase>(AIControllerClass);
		if (!AIPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to spawn AI PlayerController for Team %d"), TeamId);
			continue;
		}

		AIPC->bIsAi = true;
		
		// Spawn the AI pawn at the PlayerStart
		FTransform SpawnTransform = Start->GetActorTransform();
		APawn* AIPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform);
		if (!AIPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to spawn AI Pawn for Team %d"), TeamId);
			AIPC->Destroy();
			continue;
		}

		// Ensure AIController will not steal possession: keep PlayerController as owner
		AIPawn->AutoPossessAI = EAutoPossessAI::Disabled;
		AIPawn->AIControllerClass = nullptr;

		AIPC->Possess(AIPawn);
		AIPC->SoundMultiplier = 0.0f;

		// Ensure the AI PlayerController has a HUD (use the standard HUDClass from this GameMode)
		if (!AIPC->GetHUD())
		{
			AIPC->SpawnDefaultHUD();
			UE_LOG(LogTemp, Log, TEXT("[GM] Spawned default HUD for AI PC: %s -> HUD=%s (HUDClass=%s)"),
				*GetNameSafe(AIPC), *GetNameSafe(AIPC->GetHUD()), *GetNameSafe(HUDClass));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[GM] AI PC already has HUD: %s (HUDClass=%s)"), *GetNameSafe(AIPC->GetHUD()), *GetNameSafe(HUDClass));
		}

		AIPC->HUDBase = Cast<APathProviderHUD>(AIPC->GetHUD());
		if (!AIPC->HUDBase)
		{
			// Fallback for server‑spawned AI PCs without a local viewport: manually spawn the HUD actor and bind it
			UClass* HUDToSpawn = HUDClass ? *HUDClass : APathProviderHUD::StaticClass();
			if (!HUDToSpawn->IsChildOf(AHUD::StaticClass()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[GM] Configured HUDClass is not a HUD. Falling back to APathProviderHUD."));
				HUDToSpawn = APathProviderHUD::StaticClass();
			}
			FActorSpawnParameters HUDSpawnParams;
			HUDSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AHUD* SpawnedHUD = GetWorld()->SpawnActor<AHUD>(HUDToSpawn, FTransform::Identity, HUDSpawnParams);
			if (SpawnedHUD)
			{
				SpawnedHUD->SetOwner(AIPC);
				SpawnedHUD->PlayerOwner = AIPC;
				AIPC->HUDBase = Cast<APathProviderHUD>(SpawnedHUD);
				UE_LOG(LogTemp, Log, TEXT("[GM] Fallback-spawned HUD for AI PC: %s -> HUD=%s (HUDClass=%s)"),
					*GetNameSafe(AIPC), *GetNameSafe(SpawnedHUD), *GetNameSafe(HUDToSpawn));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[GM] Failed to fallback-spawn HUD for AI PC: %s (HUDClass=%s)"),
					*GetNameSafe(AIPC), *GetNameSafe(HUDToSpawn));
			}
		}
		
		// Spawn a non-possessing AIController that runs the Behavior Tree as an orchestrator
		UClass* OrchestratorClass = AIOrchestratorClass ? *AIOrchestratorClass : (UClass*)ARTSBTController::StaticClass();
		{
			FTransform OrchestratorTransform = FTransform::Identity;
			ARTSBTController* Orchestrator = GetWorld()->SpawnActorDeferred<ARTSBTController>(OrchestratorClass, OrchestratorTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			
			if (Orchestrator)
			{
				// Set the TeamId on the orchestrator BEFORE FinishSpawning (so it's ready in BeginPlay)
				Orchestrator->OrchestratorTeamId = TeamId;
				UGameplayStatics::FinishSpawningActor(Orchestrator, OrchestratorTransform);

				UE_LOG(LogTemp, Log, TEXT("[GM] Spawned ARTSBTController orchestrator: %s (no possession) with TeamId=%d"), *GetNameSafe(Orchestrator), TeamId);

				// If the orchestrator doesn't have a BT assigned, but the GameMode provides one, wire it up now
				if (!Orchestrator->StrategyBehaviorTree && AIBehaviorTree)
				{
					Orchestrator->StrategyBehaviorTree = AIBehaviorTree;
					UBlackboardComponent* OutBB = nullptr;
					if (Orchestrator->UseBlackboard(AIBehaviorTree->BlackboardAsset, OutBB))
					{
						const bool bStarted = Orchestrator->RunBehaviorTree(AIBehaviorTree);
						if (!bStarted)
						{
							UE_LOG(LogTemp, Error, TEXT("[GM] RunBehaviorTree failed when starting orchestrator with provided AIBehaviorTree '%s'"), *GetNameSafe(AIBehaviorTree));
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[GM] UseBlackboard failed for orchestrator. Ensure AIBehaviorTree has a Blackboard asset assigned."));
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[GM] Failed to spawn ARTSBTController orchestrator for Team %d"), TeamId);
			}
		}

		// Apply customizations and set team/waypoint
		ApplyCustomizationsFromPlayerStart(AIPC, Start);
		SetTeamIdAndDefaultWaypoint_Implementation(TeamId, Start->DefaultWaypoint, AIPC);
		OccupiedTeamIds.Add(TeamId);

		// Do same initialization as humans so the AI controller can act
		AIPC->Multi_SetMyTeamUnits(AllUnits);
		AIPC->Multi_SetCamLocation(Start->GetActorLocation());

		FName SpecificCameraUnitTagNameAI = FName(*FString::Printf(TEXT("Character.CameraUnit.%d"), PlayerIndex));
		FGameplayTag SpecificCameraUnitTagAI = FGameplayTag::RequestGameplayTag(SpecificCameraUnitTagNameAI);
		AIPC->SetCameraUnitWithTag_Implementation(SpecificCameraUnitTagAI, AIPC->SelectableTeamId);
		AIPC->Multi_SetCameraOnly();
		AIPC->Multi_HideEnemyWaypoints();
		AIPC->Multi_InitFogOfWar();
		AIPC->Multi_SetupPlayerMiniMap();
		if (AIPC->CameraBase)
		{
			AIPC->CameraBase->BlockControls = false;
		}
		AIPC->AgentInit();
		PlayerIndex++;
	}

	NavInitialisation();

	// Initialize MainHUD
	/*
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get()))
		{
			PC->Client_InitializeMainHUD();
		}
	}
	*/
	InitializeWinLoseConfigActors();
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get()))
		{
			PC->Client_InitializeWinLoseSystem();
		}
	}
}

void ARTSGameModeBase::SetupTimerFromDataTable_Implementation(FVector Location, AUnitBase* UnitToChase)
{

	for (UDataTable* UnitSpawnParameter : UnitSpawnParameters)
	{
		if (UnitSpawnParameter && HasAuthority())
		{
			TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
			for (const FName& RowName : RowNames)
			{
				FUnitSpawnParameter* SpawnParameterPtr = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));

				if (SpawnParameterPtr) 
				{
					FUnitSpawnParameter SpawnParameter = *SpawnParameterPtr; // Copy the struct


					// Use a weak pointer for the GameMode to ensure it's still valid when the timer fires
					TWeakObjectPtr<ARTSGameModeBase> WeakThis(this);
				
					// Set the timer
					if (SpawnParameter.ShouldLoop)
					{
	

						auto TimerCallback = [WeakThis, SpawnParameter, Location, UnitToChase]()
						{
							// Check if the GameMode is still valid
							if (!WeakThis.IsValid()) return;

							// Now call SpawnUnits_Implementation with all the parameters
							WeakThis->SpawnUnits_Implementation(SpawnParameter, Location, UnitToChase);
						};
					
						FTimerHandle TimerHandle;
						//SpawnTimerHandles.Add(TimerHandle);
						GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerCallback, SpawnParameter.LoopTime, true);

						FTimerHandleMapping TimerMap;
						TimerMap.Id = SpawnParameter.Id;
						TimerMap.Timer = TimerHandle;
						TimerMap.SkipTimer = false;
						SpawnTimerHandleMap.Add(TimerMap);
						TimerIndex++;
					}
				}

			}
		}
	}
}

void ARTSGameModeBase::SetupUnitsFromDataTable_Implementation(FVector Location, AUnitBase* UnitToChase, const TArray<class UDataTable*>& UnitTable) // , int TeamId , const FString& WaypointTag, int32 UnitIndex, AUnitBase* SummoningUnit, int SummonIndex
{
	for (UDataTable* UnitSpawnParameter : UnitTable)
	{
		if (UnitSpawnParameter && HasAuthority())
		{
			TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
			for (const FName& RowName : RowNames)
			{
				FUnitSpawnParameter* SpawnParameterPtr = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));

				if (SpawnParameterPtr) 
				{
					FUnitSpawnParameter SpawnParameter = *SpawnParameterPtr; // Copy the struct
					SpawnUnits_Implementation(SpawnParameter, Location, UnitToChase);
				}

			}
		}
	}
}

FTimerHandleMapping ARTSGameModeBase::GetTimerHandleMappingById(int32 SearchId)
{
	for (FTimerHandleMapping& TimerMap : SpawnTimerHandleMap)
	{
		if (TimerMap.Id == SearchId)
		{
			return TimerMap; // Return a pointer to the struct
		}
	}

	return FTimerHandleMapping(); // Return nullptr if not found
}

void ARTSGameModeBase::SetSkipTimerMappingById(int32 SearchId, bool Value)
{
	for (FTimerHandleMapping& TimerMap : SpawnTimerHandleMap)
	{
		if (TimerMap.Id == SearchId)
		{
			TimerMap.SkipTimer = Value; // Return a pointer to the struct
		}
	}
}

void ARTSGameModeBase::SpawnUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase) // , int TeamId, AWaypoint* Waypoint
{

	
	for (UDataTable* UnitSpawnParameter : UnitSpawnParameters)
	{
		if (UnitSpawnParameter)
		{
			TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
			for (const FName& RowName : RowNames)
			{
				FUnitSpawnParameter* SpawnParameter = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
				if (SpawnParameter && SpawnParameter->Id == id)
				{
					SpawnUnits_Implementation(*SpawnParameter, Location, UnitToChase);
				}
			}
		}
	}
}

AUnitBase* ARTSGameModeBase::SpawnSingleUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId,
	AWaypoint* Waypoint)
{
	for (UDataTable* UnitSpawnParameter : UnitSpawnParameters)
	{
		if (UnitSpawnParameter)
		{
			TArray<FName> RowNames = UnitSpawnParameter->GetRowNames();
			for (const FName& RowName : RowNames)
			{
				FUnitSpawnParameter* SpawnParameter = UnitSpawnParameter->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
				if (SpawnParameter && SpawnParameter->Id == id)
				{
					AUnitBase* Spawned = SpawnSingleUnit(*SpawnParameter, Location, UnitToChase, TeamId, Waypoint);

					// Send it to the producing building's rally point. Deliberately only here and not in
					// SpawnSingleUnit: this is the production entry point the abilities call, while the
					// level spawn tables go through SpawnUnits_Implementation and must keep their own
					// waypoint/patrol behaviour untouched.
					if (Spawned && !Waypoint)
					{
						ABuildingBase* Producer = nullptr;
						double BestDistSq = TNumericLimits<double>::Max();
						for (TActorIterator<ABuildingBase> It(GetWorld()); It; ++It)
						{
							ABuildingBase* Building = *It;
							if (!IsValid(Building) || Building->TeamId != Spawned->TeamId) continue;

							const double DistSq = FVector::DistSquared2D(Building->GetActorLocation(), Location);
							if (DistSq < BestDistSq)
							{
								BestDistSq = DistSq;
								Producer = Building;
							}
						}

						// Only if the spawn really came out of that building - otherwise this is some other
						// kind of summon and none of our business.
						if (Producer && BestDistSq <= FMath::Square(1500.f))
						{
							Producer->ApplyRallyPointToUnit(Spawned);
						}
					}

					return Spawned;
				}
			}
		}
	}
	return nullptr;
}

bool ARTSGameModeBase::IsUnitWithIndexDead(int32 UnitIndex)
{
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
	
		// Assuming AUnitBase has a method to check if the unit is dead
		if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead && UnitData.UnitBase->UnitIndex == UnitIndex)
		{
			// Check if the index is not already in the array
			return true;
		}
	}
	return false;
}

bool ARTSGameModeBase::RemoveDeadUnitWithIndexFromDataSet(int32 UnitIndex)
{
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
	
			// Assuming AUnitBase has a method to check if the unit is dead
			if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead && UnitData.UnitBase->UnitIndex == UnitIndex)
			{
				// Check if the index is not already in the array
			
				//UnitData.UnitBase->SaveLevelDataAndAttributes(FString::FromInt(UnitData.UnitBase->UnitIndex));
				UnitData.UnitBase->SaveAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
	
		
				AllUnits.Remove(UnitData.UnitBase);
					

				
				UnitSpawnDataSets.RemoveAt(i);
				return true;
			}
		}
	return false;
}

int32 ARTSGameModeBase::CountAliveUnitsForTeam(int32 InTeamId, bool bInvert) const
{
	int32 Count = 0;
	for (AActor* Actor : AllUnits)
	{
		const AUnitBase* Unit = Cast<AUnitBase>(Actor);
		if (!IsValid(Unit) || Unit->GetUnitState() == UnitData::Dead)
		{
			continue;
		}

		const bool bMatches = bInvert ? (Unit->TeamId != InTeamId) : (Unit->TeamId == InTeamId);
		if (bMatches)
		{
			++Count;
		}
	}
	return Count;
}

float ARTSGameModeBase::GetAdaptiveSpawnMultiplier(const FUnitSpawnParameter& SpawnParameter) const
{
	if (!SpawnParameter.bAdaptiveSpawn || SpawnParameter.AdaptiveStrength <= 0.f)
	{
		return 1.f;
	}

	const int32 Own = CountAliveUnitsForTeam(SpawnParameter.TeamId);
	const int32 Opponent = (SpawnParameter.AdaptiveOpponentTeamId >= 0)
		? CountAliveUnitsForTeam(SpawnParameter.AdaptiveOpponentTeamId)
		: CountAliveUnitsForTeam(SpawnParameter.TeamId, /*bInvert=*/true);

	// Nothing to measure against yet (opening seconds) - stay neutral rather than
	// spiking to the upper clamp on an empty battlefield.
	if (Opponent <= 0)
	{
		return 1.f;
	}

	const float Desired = FMath::Max(1.f, Opponent * FMath::Max(0.05f, SpawnParameter.AdaptiveTargetRatio));
	const float Actual = FMath::Max(1.f, static_cast<float>(Own));

	// Ratio form (not a difference): behind by 3x asks for 3x the reinforcements, ahead by
	// 3x throttles to a third. Symmetric, which is what stops either side snowballing.
	const float Multiplier = FMath::Pow(Desired / Actual, SpawnParameter.AdaptiveStrength);

	const float MinMul = FMath::Max(0.f, SpawnParameter.AdaptiveMinMultiplier);
	const float MaxMul = FMath::Max(MinMul, SpawnParameter.AdaptiveMaxMultiplier);
	return FMath::Clamp(Multiplier, MinMul, MaxMul);
}

int32 ARTSGameModeBase::CheckAndRemoveDeadUnits(int32 SpawnParaId)
{
	int32 CountOfSpecificID = 0;
	bool FoundDeadUnit = false;
	
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
		if(UnitData.Id == SpawnParaId)
		{
			// A unit leaves the roster in two ways, and both have to free the Id's budget:
			//   1. it dies normally  -> state becomes Dead (save its progression first), or
			//   2. its actor is gone -> DestroyActor / level teardown / GC.
			// Case 2 used to fall into the else branch below and keep counting as ALIVE, because the
			// only test was "pointer set AND state == Dead". Anything that clears a round with
			// DestroyActor (the Arena's AdvanceRound does exactly that) therefore leaked one phantom
			// entry per unit per round, and once the phantoms reached MaxUnitSpawnCount the row stopped
			// spawning entirely - silently, since SpawnUnits just skips the block.
			const bool bUnitGone = !IsValid(UnitData.UnitBase);
			const bool bUnitDead = !bUnitGone && UnitData.UnitBase->GetUnitState() == UnitData::Dead;

			if (bUnitGone || bUnitDead)
			{
				if (bUnitDead)
				{
					// Save data for the dead unit (do not reuse index). Only reachable while the actor
					// is still valid - a destroyed one has nothing left to save.
					UnitData.UnitBase->SaveAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
				}
				FoundDeadUnit = true;

				AllUnits.Remove(UnitData.UnitBase);

				UnitSpawnDataSets.RemoveAt(i);
			}
			else //if (UnitData.Id == SpawnParaId)
			{
				CountOfSpecificID++;
			}
		}
	}

	if(CountOfSpecificID == 0 && FoundDeadUnit)
	{
		SetSkipTimerMappingById(SpawnParaId, true);
	}

	return CountOfSpecificID;
}

AUnitBase* ARTSGameModeBase::SpawnSingleUnit(FUnitSpawnParameter SpawnParameter, FVector Location,
	AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint)
{
	if (!SpawnParameter.UnitBaseClass) return nullptr;
	// Waypointspawn
	const FVector FirstLocation = CalcLocation(SpawnParameter.UnitOffset+Location, SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);

	FTransform EnemyTransform;
	
	EnemyTransform.SetLocation(FVector(FirstLocation.X, FirstLocation.Y, SpawnParameter.UnitOffset.Z));
		
		
	const auto UnitBase = Cast<AUnitBase>
		(UGameplayStatics::BeginDeferredActorSpawnFromClass
		(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	
	
	if (UnitBase != nullptr)
	{
		if(UnitBase->UnitToChase)
		{
			UnitBase->UnitToChase = UnitToChase;
			UnitBase->SetUnitState(UnitData::Chase);
		}

		// Check and apply CharacterMesh
		if (SpawnParameter.CharacterMesh)
		{
			UnitBase->MeshAssetPath = SpawnParameter.CharacterMesh->GetPathName();
		}

		// Check and apply Material
		if (SpawnParameter.Material)
		{
			UnitBase->MeshMaterialPath = SpawnParameter.Material->GetPathName();
		}

		if (SpawnParameter.TeamId)
		{
			UnitBase->TeamId = SpawnParameter.TeamId;
		}

		if(TeamId)
		{
			UnitBase->TeamId = TeamId;
		}

		// Rows only dictate the mesh rotation when they say so; otherwise the unit's own
		// class default stands, exactly as it does for hand-placed units.
		if (SpawnParameter.bOverrideServerMeshRotation)
		{
			UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
		}
			
		UnitBase->OnRep_MeshAssetPath();
		UnitBase->OnRep_MeshMaterialPath();

		UnitBase->SetReplicateMovement(false);
		//SetReplicates(true);
		//UnitBase->GetMesh()->SetIsReplicated(true);

		// Does this have to be replicated?
		UnitBase->SetMeshRotationServer();
			
		AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);
		SeedSpawnStoredLocationFromWaypoint(UnitBase);


		if(Waypoint)
		{
			UnitBase->NextWaypoint = Waypoint;
		}
		UnitBase->UnitState = SpawnParameter.State;
		UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;

		if(SpawnParameter.SpawnAtWaypoint && UnitBase->NextWaypoint)
		{
			FVector NewLocation = CalcLocation(FVector(UnitBase->NextWaypoint->GetActorLocation().X, UnitBase->NextWaypoint->GetActorLocation().Y, UnitBase->NextWaypoint->GetActorLocation().Z+50.f), SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);
			UnitBase->SetActorLocation(NewLocation);
		}

		UnitBase->CanBeSelected = SpawnParameter.CanBeSelected;

		UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);
		
		if(SpawnParameter.Attributes)
		{
			UnitBase->DefaultAttributeEffect = SpawnParameter.Attributes;
		}

		UnitBase->InitializeAttributes();

		if (UnitBase->Attributes)
		{
			if (SpawnParameter.RunSpeed > 0)
			{
				UnitBase->Attributes->SetRunSpeed(SpawnParameter.RunSpeed);
			}
			if (SpawnParameter.BaseRunSpeed > 0)
			{
				UnitBase->Attributes->SetBaseRunSpeed(SpawnParameter.BaseRunSpeed);
			}
		}

		
		AddUnitIndexAndAssignToAllUnitsArray(UnitBase);

		UnitBase->ScheduleDelayedNavigationUpdate();
		
		return UnitBase;
	}

	return nullptr;
}


void ARTSGameModeBase::SpawnUnits_Implementation(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase) // , int TeamId, AWaypoint* Waypoint, int32 UnitIndex, AUnitBase* SummoningUnit, int SummonIndex
{

	if (!SpawnParameter.UnitBaseClass) return;
	
	int UnitCount = CheckAndRemoveDeadUnits(SpawnParameter.Id);

	// Adaptive reinforcement. Returns exactly 1.0 when the row has bAdaptiveSpawn off, so
	// the effective values below are identical to the raw ones for every existing table.
	const float AdaptiveMultiplier = GetAdaptiveSpawnMultiplier(SpawnParameter);
	const int32 EffectiveMaxUnitSpawnCount = SpawnParameter.bAdaptiveSpawn && SpawnParameter.bAdaptiveScalesMaxCount
		? FMath::Max(1, FMath::RoundToInt(SpawnParameter.MaxUnitSpawnCount * AdaptiveMultiplier))
		: SpawnParameter.MaxUnitSpawnCount;
	const int32 EffectiveUnitCount = SpawnParameter.bAdaptiveSpawn
		? FMath::Max(1, FMath::RoundToInt(SpawnParameter.UnitCount * AdaptiveMultiplier))
		: SpawnParameter.UnitCount;

	FTimerHandleMapping TimerMap = GetTimerHandleMappingById(SpawnParameter.Id);
	if(UnitCount < EffectiveMaxUnitSpawnCount && TimerMap.SkipTimer && SpawnParameter.SkipTimerAfterDeath){
		SetSkipTimerMappingById(SpawnParameter.Id, false);
		return;
	}

	HighestSquadId++;

	if(UnitCount < EffectiveMaxUnitSpawnCount)
	{
		HighestSquadId++;
		int RandomCount = FMath::RandRange(SpawnParameter.MinRandomCount, SpawnParameter.MaxRandomCount);
		for(int i = 0; i < EffectiveUnitCount + RandomCount; i++)
		{
			// Waypointspawn
			const FVector FirstLocation = CalcLocation(SpawnParameter.UnitOffset+Location, SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);

			FTransform EnemyTransform;
			EnemyTransform.SetLocation(FVector(FirstLocation.X, FirstLocation.Y, SpawnParameter.UnitOffset.Z));
			
			const auto UnitBase = Cast<AUnitBase>
				(UGameplayStatics::BeginDeferredActorSpawnFromClass
				(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

			
			/*
			if (SpawnParameter.UnitControllerBaseClass)
			{
				AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
				ControllerBase->Possess(UnitBase);
			}*/

			
			if (UnitBase != nullptr)
			{
				if(UnitToChase != nullptr)
				{
					UnitBase->UnitToChase = UnitToChase;
					UnitBase->SetUnitState(UnitData::Chase);
				}

				// Check and apply CharacterMesh
				if (SpawnParameter.CharacterMesh)
				{
					UnitBase->MeshAssetPath = SpawnParameter.CharacterMesh->GetPathName();
				}

				// Check and apply Material
				if (SpawnParameter.Material)
				{
					UnitBase->MeshMaterialPath = SpawnParameter.Material->GetPathName();
				}
				
				if (SpawnParameter.TeamId)
				{
					UnitBase->TeamId = SpawnParameter.TeamId;
				}

				// Rows only dictate the mesh rotation when they say so; otherwise the unit's own
				// class default stands, exactly as it does for hand-placed units.
				if (SpawnParameter.bOverrideServerMeshRotation)
				{
					UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
				}
				
				UnitBase->OnRep_MeshAssetPath();
				UnitBase->OnRep_MeshMaterialPath();

				//UnitBase->SetReplicateMovement(true);
				//UnitBase->SetReplicates(true);
				//UnitBase->GetMesh()->SetIsReplicated(true);
				
				UnitBase->SetMeshRotationServer();
				
				AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);
				SeedSpawnStoredLocationFromWaypoint(UnitBase);

				// Reicht den Zeilen-Schalter an die Einheit weiter. Ohne einen
				// AIdlePatrolEnforcer im Level bleibt das Flag folgenlos.
				UnitBase->bPreventIdling = SpawnParameter.bPreventIdling;

				/*
				if(Waypoint != nullptr)
				{
					UnitBase->NextWaypoint = Waypoint;
				}*/
				
				UnitBase->UnitState = SpawnParameter.State;
				UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
				
				// Assign SquadId only when the DataTable row specifies squad spawning
				if (SpawnParameter.SpawnAsSquad)
				{
					UnitBase->SquadId = HighestSquadId;
				}
				else
				{
					UnitBase->SquadId = 0;
				}
				// Ensure proper healthbar ownership/state after squad decision
				UnitBase->EnsureSquadHealthbarState();
				if(SpawnParameter.SpawnAtWaypoint && UnitBase->NextWaypoint)
				{
					FVector NewLocation = CalcLocation(FVector(UnitBase->NextWaypoint->GetActorLocation().X, UnitBase->NextWaypoint->GetActorLocation().Y, UnitBase->NextWaypoint->GetActorLocation().Z+50.f), SpawnParameter.UnitMinRange, SpawnParameter.UnitMaxRange);
					UnitBase->SetActorLocation(NewLocation);
				}

				UnitBase->CanBeSelected = SpawnParameter.CanBeSelected;
				
				UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);

				/*
				APlayerController* MyPC = GetWorld()->GetFirstPlayerController();
				if (MyPC)
				{
					UnitBase->SpawnFogOfWarManagerTeamIndependent(MyPC);
				}
				*/
				
				
				if(SpawnParameter.Attributes)
				{
					UnitBase->DefaultAttributeEffect = SpawnParameter.Attributes;
				}
			
				UnitBase->InitializeAttributes();

				if (UnitBase->Attributes)
				{
					if (SpawnParameter.RunSpeed > 0)
					{
						UnitBase->Attributes->SetRunSpeed(SpawnParameter.RunSpeed);
					}
					if (SpawnParameter.BaseRunSpeed > 0)
					{
						UnitBase->Attributes->SetBaseRunSpeed(SpawnParameter.BaseRunSpeed);
					}
				}
			
				//UnitBase->MassActorBindingComponent->SetupMassOnUnit();
				// Assign a new unique UnitIndex without reusing old ones
				AddUnitIndexAndAssignToAllUnitsArrayWithIndex(UnitBase, INDEX_NONE, SpawnParameter);

				FUnitSpawnData UnitSpawnDataSet;
				UnitSpawnDataSet.Id = SpawnParameter.Id;
				UnitSpawnDataSet.UnitBase = UnitBase;
				UnitSpawnDataSet.SpawnParameter = SpawnParameter;

				UnitBase->ScheduleDelayedNavigationUpdate();
				
				UnitSpawnDataSets.Add(UnitSpawnDataSet);
			}
			
		}
	}
	// Enemyspawn
}

int ARTSGameModeBase::AssignNewHighestIndex(AUnitBase* Unit)
{
	HighestUnitIndex++;
	Unit->SetUnitIndex(HighestUnitIndex);
	return HighestUnitIndex;
}

int ARTSGameModeBase::AddUnitIndexAndAssignToAllUnitsArray(AUnitBase* UnitBase)
{
	int Index = AssignNewHighestIndex(UnitBase);
	AllUnits.Add(UnitBase);
	return Index;
}

void ARTSGameModeBase::AddUnitIndexAndAssignToAllUnitsArrayWithIndex(AUnitBase* UnitBase, int32 /*Index*/, FUnitSpawnParameter /*SpawnParameter*/)
{
	// Always assign a new unique UnitIndex and add to AllUnits. Do not reuse indices.
	AssignNewHighestIndex(UnitBase);
	AllUnits.Add(UnitBase);
}


// Seeds AMassUnitBase::SpawnStoredLocation with a random point inside the unit's waypoint area.
// StoredLocation is the anchor every return-to-post path uses; leaving it on the raw spawn
// position is what made units walk back to spawn after a fight. Randomised inside
// PatrolCloseOffset so a whole spawn wave does not share one anchor and pile up on it.
static void SeedSpawnStoredLocationFromWaypoint(AUnitBase* UnitBase)
{
	AMassUnitBase* MassUnit = Cast<AMassUnitBase>(UnitBase);
	if (!MassUnit || !UnitBase->NextWaypoint)
	{
		return;
	}

	const FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();
	const FVector2D Offset = UnitBase->NextWaypoint->PatrolCloseOffset;
	const float Radius = FMath::Max(0.f, (Offset.X + Offset.Y) * 0.5f);

	if (Radius <= KINDA_SMALL_NUMBER)
	{
		MassUnit->SpawnStoredLocation = WaypointLocation;
		return;
	}

	// sqrt keeps the points evenly spread over the disc instead of bunching in the middle.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float R = Radius * FMath::Sqrt(FMath::FRand());
	const FVector Gestreut = WaypointLocation + FVector(R * FMath::Cos(Angle), R * FMath::Sin(Angle), 0.f);

	// Auf das Navigationsnetz ziehen. Zweite Kopie derselben Streuformel - die erste steht in
	// GetPatrolHomeLocation und wurde am 19.08. bereits projiziert. Diese hier blieb zunaechst
	// unberuehrt, weshalb nach dem ersten Eingriff weiterhin 200-500 Fehlanfragen je Partie uebrig
	// blieben, alle mit einem Ankerpunkt ausserhalb des NavMeshBoundsVolume.
	//
	// Der Wegpunkt im Testlevel liegt bei (-10191, 8512), das Volumen endet bei X=-12455: bei einem
	// PatrolCloseOffset von rund 10000 (Radius 5000) landet ein Teil der Anker draussen. Jeder
	// Rueckweg dorthin scheitert und wird jeden Takt wiederholt.
	//
	// Hier ist keine Determinismus-Zusage einzuhalten (die Streuung nutzt ohnehin FRand), die
	// Projektion darf also frei verschieben. Schlaegt sie fehl, bleibt es beim gestreuten Punkt.
	MassUnit->SpawnStoredLocation = Gestreut;
	if (UWorld* Welt = UnitBase->GetWorld())
	{
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Welt))
		{
			FNavLocation Projiziert;
			if (NavSys->ProjectPointToNavigation(Gestreut, Projiziert, FVector(Radius, Radius, 1000.f)))
			{
				MassUnit->SpawnStoredLocation = Projiziert.Location;
			}
		}
	}
}

void ARTSGameModeBase::AssignWaypointToUnit(AUnitBase* UnitBase, const FString& WaypointTag)
{
	if (UnitBase == nullptr)
	{
		return;
	}

	// Find Waypoints with the specified tag
	TArray<AActor*> FoundWaypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), FoundWaypoints);

	// Filter waypoints by tag
	AWaypoint* SelectedWaypoint = nullptr;
	for (AActor* Actor : FoundWaypoints)
	{
		AWaypoint* Waypoint = Cast<AWaypoint>(Actor);
		if (Waypoint && Waypoint->Tag == WaypointTag)
		{
			SelectedWaypoint = Waypoint;
			break; // or choose waypoints based on some other criteria
		}
	}

	// Assign the found waypoint to the unit
	if (SelectedWaypoint != nullptr)
	{
		UnitBase->NextWaypoint = SelectedWaypoint;
	}

}


FVector ARTSGameModeBase::CalcLocation(FVector Offset, FVector MinRange, FVector MaxRange)
{
	int MultiplierA;
	const float randChooserA = FMath::RandRange(1, 10);
	if(randChooserA <= 5)
	{
		MultiplierA = -1;
	}else
	{
		MultiplierA = 1;
	}

	int MultiplierB;
	const float randChooserB = FMath::RandRange(1, 10);
	if(randChooserB <= 5)
	{
		MultiplierB = -1;
	}else
	{
		MultiplierB = 1;
	}
				
	const float RandomOffsetX = FMath::RandRange(MinRange.X, MaxRange.X);
	const float RandomOffsetY = FMath::RandRange(MinRange.Y, MaxRange.Y);
	const float RandomOffsetZ = FMath::RandRange(MinRange.Z, MaxRange.Z);
				
	const float X = RandomOffsetX*MultiplierA+Offset.X; 
	const float Y = RandomOffsetY*MultiplierB+Offset.Y; 
	const float Z = RandomOffsetZ+Offset.Z;

	FVector Ergebnis(X, Y, Z);

	// BEHEBUNG (22.08.2026): Spawnpunkte auf das Navigationsnetz ziehen.
	//
	// Die Streuung oben wuerfelt frei im Rechteck MinRange..MaxRange, ohne zu pruefen, ob der
	// getroffene Punkt ueberhaupt begehbar ist. Im Prologue ist das Netz eine schmale, kurvige
	// Strasse zwischen Asteroidenfeldern - dort landet ein grosser Teil der Wuerfe daneben.
	// Eine Einheit neben dem Netz findet keinen Weg, steht still und wirkt im Spiel wie
	// "gespawnt und wieder verschwunden".
	//
	// Schlaegt die Projektion fehl (kein Navigationssystem, Level ganz ohne NavMesh), bleibt der
	// gewuerfelte Punkt unveraendert - der Spawn darf daran nicht scheitern.
	//
	// Die Hoehe bleibt unangetastet: die Aufrufer setzen Z selbst (aus UnitOffset.Z bzw. der
	// Wegpunkthoehe), und eine vom Netz zurueckgemeldete Z-Hoehe wuerde fliegende Einheiten
	// auf den Boden ziehen.
	if (UWorld* Welt = GetWorld())
	{
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Welt))
		{
			const FVector Suchbereich(
				FMath::Max(MaxRange.X, 1000.f),
				FMath::Max(MaxRange.Y, 1000.f),
				1000.f);

			FNavLocation Projiziert;
			if (NavSys->ProjectPointToNavigation(Ergebnis, Projiziert, Suchbereich))
			{
				Ergebnis.X = Projiziert.Location.X;
				Ergebnis.Y = Projiziert.Location.Y;
			}
		}
	}

	return Ergebnis;
}

void ARTSGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::LevelTransition || EndPlayReason == EEndPlayReason::Quit)
	{
		// Clean up AI-specific actors that might not be handled by the engine transition
		TArray<AActor*> Orchestrators;
		UGameplayStatics::GetAllActorsOfClass(this, ARTSBTController::StaticClass(), Orchestrators);
		for (AActor* Actor : Orchestrators)
		{
			if (Actor)
			{
				Actor->Destroy();
			}
		}

		// AI PlayerControllers are handled in HandleSeamlessTravelPlayer for seamless travel,
		// but we can ensure they are gone here as well if they were missed or for non-seamless.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			ACameraControllerBase* PC = Cast<ACameraControllerBase>(It->Get());
			if (PC && PC->bIsAi)
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					Pawn->Destroy();
				}
				PC->Destroy();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
	/*
	#if WITH_EDITOR
		// Get the world context
		if (UWorld* World = GetWorld())
		{
			// Get your custom subsystem
			if (URTSMassEntitySubsystem* MassSubsystem = World->GetSubsystem<URTSMassEntitySubsystem>())
			{
				// Call the reset function we created
				MassSubsystem->ForceResetForPIE();
			}
		}
	#endif
	*/
}
