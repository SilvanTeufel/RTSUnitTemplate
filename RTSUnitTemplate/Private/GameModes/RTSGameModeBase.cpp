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
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"
#include "Controller/AIController/BuildingControllerBase.h"
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


void ARTSGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	bWinLoseTriggered = false;

	FillUnitArrays();

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

	FTimerHandle TimerHandleGatherController;
	GetWorldTimerManager().SetTimer(TimerHandleGatherController, this, &ARTSGameModeBase::SetTeamIdsAndWaypoints, GatherControllerTimer, false);

	// Show loading widget via GameState (robust for late joiners)
	if (LoadingWidgetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ARTSGameModeBase::BeginPlay: Triggering loading widget in GameState."));
		if (AResourceGameState* GS = GetGameState<AResourceGameState>())
		{
			const float WidgetDuration = FMath::Max(5.f, (float)GatherControllerTimer + 1.f);
			const int32 NewTriggerId = FMath::RandRange(1, 2147483647);

			GS->LoadingWidgetConfig.WidgetClass = LoadingWidgetClass;
			GS->LoadingWidgetConfig.Duration = WidgetDuration;
			GS->LoadingWidgetConfig.TriggerId = NewTriggerId;
			GS->LoadingWidgetConfig.ServerWorldTimeStart = GS->GetServerWorldTimeSeconds();
			GS->MatchStartTime = GS->LoadingWidgetConfig.ServerWorldTimeStart + WidgetDuration;

			UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ARTSGameModeBase::BeginPlay: GameState config set. Duration: %f, TriggerId: %d, StartTime: %f, MatchStartTime: %f"), 
				WidgetDuration, NewTriggerId, GS->LoadingWidgetConfig.ServerWorldTimeStart, GS->MatchStartTime);

			// Trigger for local host immediately
			GS->OnRep_LoadingWidgetConfig();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[DEBUG_LOG] ARTSGameModeBase::BeginPlay: Failed to get AResourceGameState! Current GameState is %s"), 
				GetWorld()->GetGameState() ? *GetWorld()->GetGameState()->GetClass()->GetName() : TEXT("Null"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DEBUG_LOG] RTSGameModeBase: LoadingWidgetClass is not set in GameMode!"));
	}

	FTimerHandle TimerHandleStartDataTable;
	GetWorldTimerManager().SetTimer(TimerHandleStartDataTable, this, &ARTSGameModeBase::DataTableTimerStart, GatherControllerTimer+5.f+DelaySpawnTableTime, false);

	GetWorldTimerManager().SetTimer(WinLoseTimerHandle, this, &ARTSGameModeBase::CheckWinLoseConditionTimer, 1.0f, true);
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
}

void ARTSGameModeBase::CheckWinLoseCondition(AUnitBase* DestroyedUnit)
{
	if (!bInitialSpawnFinished || GetWorld()->GetTimeSeconds() < (float)GatherControllerTimer + 10.f + DelaySpawnTableTime) return;
	if (bWinLoseTriggered) return;
	if (WinLoseConfigActors.Num() == 0) return;

	TArray<ABuildingBase*> AllBuildings;
	bool bNeedBuildings = false;
	for (AWinLoseConfigActor* Config : WinLoseConfigActors)
	{
		if (Config->GetCurrentWinCondition() == EWinLoseCondition::AllBuildingsDestroyed || 
			Config->LoseCondition == EWinLoseCondition::AllBuildingsDestroyed)
		{
			bNeedBuildings = true;
			break;
		}
	}

	if (bNeedBuildings)
	{
		for (TActorIterator<ABuildingBase> It(GetWorld()); It; ++It)
		{
			ABuildingBase* Building = *It;
			if (Building && Building != DestroyedUnit && Building->GetUnitState() != UnitData::Dead)
			{
				AllBuildings.Add(Building);
			}
		}

		if (!bBuildingsEverExisted && AllBuildings.Num() > 0)
		{
			bBuildingsEverExisted = true;
		}

		if (!bBuildingsEverExisted) return; // Wait until at least one building is detected
	}

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
			if (AdvancedConfigs.Contains(Config)) continue;
			if (Config->TeamId != 0 && Config->TeamId != PlayerTeamId) continue;

			bool bLocalTriggered = false;
			bool bWon = false;

			// 1. Check Win Condition
			EWinLoseCondition CurrentWinCondition = Config->GetCurrentWinCondition();
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
					bLocalTriggered = true;
					bWon = true;
				}
			}
			else if (CurrentWinCondition == EWinLoseCondition::TaggedUnitDestroyed)
			{
				if (DestroyedUnit && DestroyedUnit->UnitTags.HasTagExact(Config->WinLoseTargetTag))
				{
					bLocalTriggered = true;
					bWon = (DestroyedUnit->TeamId != PlayerTeamId);
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

				if (CurrentTime >= Config->TargetGameTime)
				{
					bLocalTriggered = true;
					bWon = (PlayerTeamId == Config->TeamId || Config->TeamId == 0);
				}
			}

			// Sequential win logic
			if (bLocalTriggered && bWon)
			{
				if (!Config->IsLastWinCondition())
				{
					Config->AdvanceToNextWinCondition();
					AdvancedConfigs.Add(Config);
					bLocalTriggered = false; // Don't end yet
					bWon = false;
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
				else if (Config->LoseCondition == EWinLoseCondition::TaggedUnitDestroyed)
				{
					if (DestroyedUnit && DestroyedUnit->UnitTags.HasTagExact(Config->WinLoseTargetTag))
					{
						if (DestroyedUnit->TeamId == PlayerTeamId)
						{
							bLocalTriggered = true;
							bWon = false;
						}
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

	if (bAnyWon && WinLoseConfigActor) WinLoseConfigActor->OnYouWonTheGame.Broadcast();
	if (bAnyLost && WinLoseConfigActor) WinLoseConfigActor->OnYouLostTheGame.Broadcast();
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

                // Use the controlled Pawn as the navigation agent.
                APawn* Pawn = World->GetFirstPlayerController()->GetPawn();
                if (!Pawn)
                {
                    UE_LOG(LogTemp, Warning, TEXT("No Pawn found to use as navigation agent."));
                    return;
                }
                
                INavAgentInterface* NavAgent = Cast<INavAgentInterface>(Pawn);
                if (!NavAgent)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Pawn does not implement INavAgentInterface."));
                    return;
                }


            	// Create a default query filter
            	FSharedConstNavQueryFilter QueryFilter = NavData->GetQueryFilter(UNavigationQueryFilter::StaticClass());
            	
            	
                // Now create the dummy pathfinding query with the proper parameters.
                FPathFindingQuery DummyQuery(*NavAgent, *NavData, PointA.Location, PointB.Location, QueryFilter);
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
            UE_LOG(LogTemp, Warning, TEXT("Default NavData instance not found for warmup."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Navigation System not found for warmup."));
    }
}


void ARTSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ARTSGameModeBase::PostLogin: %s"), NewPlayer ? *NewPlayer->GetName() : TEXT("None"));
	
	if (NewPlayer)
	{
		SetupLoadingWidgetForPlayer(NewPlayer);
	}
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
					UE_LOG(LogTemp, Log, TEXT("[DEBUG_LOG] ARTSGameModeBase::SetupLoadingWidgetForPlayer: Triggering for %s. Remaining: %f"), *NewPlayer->GetName(), Remaining);
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
	  UClass* OrchestratorClass = AIOrchestratorClass ? *AIOrchestratorClass : ARTSBTController::StaticClass();
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   AActor* OrchestratorActor = GetWorld()->SpawnActor<AActor>(OrchestratorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			ARTSBTController* Orchestrator = Cast<ARTSBTController>(OrchestratorActor);
			if (Orchestrator)
		{
			UE_LOG(LogTemp, Log, TEXT("[GM] Spawned ARTSBTController orchestrator: %s (no possession)"), *GetNameSafe(Orchestrator));

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
			
					return SpawnSingleUnit(*SpawnParameter, Location, UnitToChase, TeamId, Waypoint);
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

int32 ARTSGameModeBase::CheckAndRemoveDeadUnits(int32 SpawnParaId)
{
	int32 CountOfSpecificID = 0;
	bool FoundDeadUnit = false;
	
	for (int32 i = UnitSpawnDataSets.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = UnitSpawnDataSets[i];
		if(UnitData.Id == SpawnParaId)
		{
			// Assuming AUnitBase has a method to check if the unit is dead
			if (UnitData.UnitBase && UnitData.UnitBase->GetUnitState() == UnitData::Dead)
			{
				// Save data for the dead unit (do not reuse index)
				UnitData.UnitBase->SaveAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
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

		UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			
		UnitBase->OnRep_MeshAssetPath();
		UnitBase->OnRep_MeshMaterialPath();

		UnitBase->SetReplicateMovement(false);
		//SetReplicates(true);
		//UnitBase->GetMesh()->SetIsReplicated(true);

		// Does this have to be replicated?
		UnitBase->SetMeshRotationServer();
			
		AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);


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


	FTimerHandleMapping TimerMap = GetTimerHandleMappingById(SpawnParameter.Id);
	if(UnitCount < SpawnParameter.MaxUnitSpawnCount && TimerMap.SkipTimer && SpawnParameter.SkipTimerAfterDeath){
		SetSkipTimerMappingById(SpawnParameter.Id, false);
		return;
	}
	
	HighestSquadId++;
	
	if(UnitCount < SpawnParameter.MaxUnitSpawnCount)
	{
		HighestSquadId++;
		int RandomCount = FMath::RandRange(SpawnParameter.MinRandomCount, SpawnParameter.MaxRandomCount);
		for(int i = 0; i < SpawnParameter.UnitCount + RandomCount; i++)
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

				UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
				
				UnitBase->OnRep_MeshAssetPath();
				UnitBase->OnRep_MeshMaterialPath();

				//UnitBase->SetReplicateMovement(true);
				//UnitBase->SetReplicates(true);
				//UnitBase->GetMesh()->SetIsReplicated(true);
				
				UnitBase->SetMeshRotationServer();
				
				AssignWaypointToUnit(UnitBase, SpawnParameter.WaypointTag);

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
				
	return FVector(X, Y, Z);
}

void ARTSGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
