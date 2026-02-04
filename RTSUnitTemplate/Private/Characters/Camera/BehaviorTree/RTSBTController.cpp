#include "Characters/Camera/BehaviorTree/RTSBTController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ARTSBTController::ARTSBTController()
{
    PrimaryActorTick.bCanEverTick = true; // Enable if you want to poll BB each frame
    OrchestratorTeamId = -1;
    // Increase watchdog timeout to avoid premature fallback during startup
    BBServiceWatchdogTimeout = 2.0f;
}

void ARTSBTController::BeginPlay()
{
    Super::BeginPlay();

    // Ensure BT is started even when we don't possess a pawn (orchestrator mode)
    if (StrategyBehaviorTree)
    {
        if (!BehaviorComp)
        {
            UBlackboardComponent* OutBB = nullptr;
            if (UseBlackboard(StrategyBehaviorTree->BlackboardAsset, OutBB))
            {
                BlackboardComp = OutBB;
                const bool bStarted = RunBehaviorTree(StrategyBehaviorTree);
                BehaviorComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
                if (bStarted)
                {
                    if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Behavior Tree started in BeginPlay: %s"), *StrategyBehaviorTree->GetName());
                }
                else
                {
                    if (bDebug) UE_LOG(LogTemp, Error, TEXT("RTSBTController: RunBehaviorTree failed in BeginPlay for '%s'"), *GetNameSafe(StrategyBehaviorTree));
                }

                // Do not auto-start controller-owned BB updates unless enabled
                StartControllerBBUpdates();
            }
            else
            {
                if (bDebug) UE_LOG(LogTemp, Error, TEXT("RTSBTController: UseBlackboard failed in BeginPlay. Ensure the BT's Blackboard asset is set."));
            }
        }
    }
}

void ARTSBTController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("!!!!!!!RTSBTController: OnPossess!!!!"));

    if (!ensure(InPawn))
    {
        return;
    }

    // Optional: ensure the pawn has an inference component (used by your BT task)
    if (!InPawn->FindComponentByClass<UInferenceComponent>())
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RTSBTController: Possessed pawn has no UInferenceComponent. BT tasks like BTT_ChooseActionByIndex may require it."));
    }

    if (StrategyBehaviorTree)
    {
        UBlackboardComponent* OutBB = nullptr;
        if (UseBlackboard(StrategyBehaviorTree->BlackboardAsset, OutBB))
        {
            BlackboardComp = OutBB;
            const bool bStarted = RunBehaviorTree(StrategyBehaviorTree);
            if (!bStarted)
            {
                if (bDebug) UE_LOG(LogTemp, Error, TEXT("RTSBTController: RunBehaviorTree failed for '%s'"), *StrategyBehaviorTree->GetName());
            }
            BehaviorComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());

            if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Behavior Tree started: %s"), *StrategyBehaviorTree->GetName());

            // Start controller-owned Blackboard updates based on mode
            StartControllerBBUpdates();
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Error, TEXT("RTSBTController: UseBlackboard failed. Ensure the BT's Blackboard asset is set."));
        }
    }
    else
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RTSBTController: StrategyBehaviorTree is not assigned."));
    }
}

void ARTSBTController::OnUnPossess()
{
    // Stop updates
    PauseBBUpdates();

    if (BehaviorComp)
    {
        BehaviorComp->StopTree(EBTStopMode::Safe);
    }
    if (BBUpdateTimerHandle.IsValid())
    {
        GetWorldTimerManager().ClearTimer(BBUpdateTimerHandle);
    }

    Super::OnUnPossess();
}


void ARTSBTController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Drive Blackboard updates from Tick if configured
    if (bEnableControllerBBUpdates && bUseTickForBBUpdates)
    {
        BBUpdateAccumulator += DeltaSeconds;
        if (BBUpdateAccumulator >= BlackboardUpdateInterval)
        {
            // Call once per interval and keep leftover for cadence stability
            const float Leftover = FMath::Fmod(BBUpdateAccumulator, BlackboardUpdateInterval);
            BBUpdateAccumulator = Leftover;
            PushBlackboardFromGameState_ControllerOwned();
        }
    }

    // Watchdog: if service hasn't pinged within timeout, auto-enable controller BB updates
    if (!bEnableControllerBBUpdates && bAutoFallbackToControllerBBUpdates)
    {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        const bool bTimedOut = (LastBBServiceTickTime < 0.0) || ((Now - LastBBServiceTickTime) > BBServiceWatchdogTimeout);
        if (bTimedOut && !bControllerBBFallbackEngaged)
        {
            bEnableControllerBBUpdates = true;
            bControllerBBFallbackEngaged = true;
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BT Service watchdog timed out (last=%.3f, now=%.3f, timeout=%.3f). Enabling controller-owned Blackboard updates."), LastBBServiceTickTime, Now, BBServiceWatchdogTimeout);
            StartControllerBBUpdates();
        }
    }

    // Optional dispatch: read and clear SelectedActionJSON
    const FString Action = ConsumeSelectedActionJSON();
    if (!Action.IsEmpty())
    {
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Consumed SelectedActionJSON: %s"), *Action);

        // Dispatch action to the RLAgent's InferenceComponent to actually execute it
        if (UWorld* World = GetWorld())
        {
            TArray<AActor*> Found;
            UGameplayStatics::GetAllActorsOfClass(World, ARLAgent::StaticClass(), Found);
            ARLAgent* TargetedAgent = nullptr;
            for (AActor* Actor : Found)
            {
                if (ARLAgent* Candidate = Cast<ARLAgent>(Actor))
                {
                    // If we have a team ID, try to find the agent belonging to that team
                    if (AExtendedControllerBase* CandidatePC = Cast<AExtendedControllerBase>(Candidate->GetController()))
                    {
                        if (CandidatePC->SelectableTeamId == OrchestratorTeamId)
                        {
                            TargetedAgent = Candidate;
                            break;
                        }
                    }
                }
            }

            // Fallback to first one if no team match found
            if (!TargetedAgent && Found.Num() > 0)
            {
                TargetedAgent = Cast<ARLAgent>(Found[0]);
            }

            if (TargetedAgent)
            {
                if (UInferenceComponent* Inf = TargetedAgent->FindComponentByClass<UInferenceComponent>())
                {
                    Inf->ExecuteActionFromJSON(Action);
                }
                else
                {
                    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RTSBTController: Targeted RLAgent has no UInferenceComponent; cannot execute action."));
                }
            }
        }
    }
}

void ARTSBTController::PauseBBUpdates()
{
    if (!bUseTickForBBUpdates && BBUpdateTimerHandle.IsValid())
    {
        GetWorldTimerManager().PauseTimer(BBUpdateTimerHandle);
    }
}

void ARTSBTController::ResumeBBUpdates()
{
    if (!bUseTickForBBUpdates && BBUpdateTimerHandle.IsValid())
    {
        GetWorldTimerManager().UnPauseTimer(BBUpdateTimerHandle);
    }
}

void ARTSBTController::PushBlackboardFromGameState_ControllerOwned()
{
    // Entry log (rate-limited)
    static int32 EntryCounter = 0;
    EntryCounter++;
    if (bDebug && EntryCounter % 10 == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: BB updater tick entered (count=%d)"), EntryCounter);
    }

    // Early return logs (rate-limited per second)
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    if (!BlackboardComp)
    {
        if (bDebug && (Now - LastBBWarnTime > 1.0))
        {
            UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BB updater early return: BlackboardComp is null."));
            LastBBWarnTime = Now;
        }
        return;
    }
    
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        if (bRequirePossessedPawnForBBUpdates)
        {
            if (bDebug && (Now - LastPawnWarnTime > 1.0))
            {
                UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BB updater early return: Pawn is null (not possessing a pawn yet)."));
                LastPawnWarnTime = Now;
            }
            return;
        }
        else
        {
            // Informative (throttled) but continue without a pawn
            if (bDebug && (Now - LastPawnWarnTime > 1.0))
            {
                UE_LOG(LogTemp, Warning, TEXT("RTSBTController: Proceeding with BB update without a possessed Pawn (bRequirePossessedPawnForBBUpdates=false)."));
                LastPawnWarnTime = Now;
            }
        }
    }

    // Locate a RLAgent in the world to gather game state
    ARLAgent* RLAgent = nullptr;
    int32 TeamId = OrchestratorTeamId;

    if (UWorld* World = GetWorld())
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(World, ARLAgent::StaticClass(), Found);
        
        for (AActor* Actor : Found)
        {
            if (ARLAgent* Candidate = Cast<ARLAgent>(Actor))
            {
                // If we don't have a team ID yet, pick the first one we find
                if (TeamId < 0)
                {
                    RLAgent = Candidate;
                    break;
                }

                // If we have a team ID, find the agent belonging to that team
                if (AExtendedControllerBase* CandidatePC = Cast<AExtendedControllerBase>(Candidate->GetController()))
                {
                    if (CandidatePC->SelectableTeamId == TeamId)
                    {
                        RLAgent = Candidate;
                        break;
                    }
                }
            }
        }

        // Fallback to first found if no team match
        if (!RLAgent && Found.Num() > 0)
        {
            RLAgent = Cast<ARLAgent>(Found[0]);
        }
    }

    if (!RLAgent)
    {
        if (bDebug && (Now - LastPawnWarnTime > 1.0))
        {
            UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BB updater early return: No ARLAgent found in world to provide GameState."));
            LastPawnWarnTime = Now;
        }
        return;
    }

    if (TeamId < 0)
    {
        AExtendedControllerBase* PlayerCtrl = Cast<AExtendedControllerBase>(RLAgent->GetController());
        if (PlayerCtrl)
        {
            TeamId = PlayerCtrl->SelectableTeamId;
        }
    }

    if (TeamId < 0)
    {
        return;
    }

    // Gather the latest game state via RLAgent's existing helper
    const FGameStateData GS = RLAgent->GatherGameState(TeamId);

    auto SafeSetBBFloat = [&](FName KeyName, float Value)
    {
        if (KeyName.IsNone()) return;
        if (BlackboardComp->GetKeyID(KeyName) == FBlackboard::InvalidKey)
        {
            return;
        }
        BlackboardComp->SetValueAsFloat(KeyName, Value);
    };

    auto SafeSetBBInt = [&](FName KeyName, int32 Value)
    {
        if (KeyName.IsNone()) return;
        if (BlackboardComp->GetKeyID(KeyName) == FBlackboard::InvalidKey)
        {
            return;
        }
        BlackboardComp->SetValueAsInt(KeyName, Value);
    };

    // Write required keys for Rule-Based task
    SafeSetBBInt(TEXT("TeamId"), TeamId);
    SafeSetBBInt(TEXT("MyUnitCount"), GS.MyUnitCount);
    SafeSetBBInt(TEXT("EnemyUnitCount"), GS.EnemyUnitCount);
    SafeSetBBFloat(TEXT("MyTotalHealth"), GS.MyTotalHealth);
    SafeSetBBFloat(TEXT("EnemyTotalHealth"), GS.EnemyTotalHealth);
    
    SafeSetBBFloat(TEXT("PrimaryResource"), GS.PrimaryResource);
    SafeSetBBFloat(TEXT("SecondaryResource"), GS.SecondaryResource);
    SafeSetBBFloat(TEXT("TertiaryResource"), GS.TertiaryResource);
    SafeSetBBFloat(TEXT("RareResource"), GS.RareResource);
    SafeSetBBFloat(TEXT("EpicResource"), GS.EpicResource);
    SafeSetBBFloat(TEXT("LegendaryResource"), GS.LegendaryResource);

    SafeSetBBFloat(TEXT("MaxPrimaryResource"), GS.MaxPrimaryResource);
    SafeSetBBFloat(TEXT("MaxSecondaryResource"), GS.MaxSecondaryResource);
    SafeSetBBFloat(TEXT("MaxTertiaryResource"), GS.MaxTertiaryResource);
    SafeSetBBFloat(TEXT("MaxRareResource"), GS.MaxRareResource);
    SafeSetBBFloat(TEXT("MaxEpicResource"), GS.MaxEpicResource);
    SafeSetBBFloat(TEXT("MaxLegendaryResource"), GS.MaxLegendaryResource);

    BlackboardComp->SetValueAsVector(TEXT("AgentPosition"), GS.AgentPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageFriendlyPosition"), GS.AverageFriendlyPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageEnemyPosition"), GS.AverageEnemyPosition);

    // Per-tag unit counts (friendly/enemy)
    SafeSetBBInt(TEXT("Alt1TagFriendlyUnitCount"), GS.Alt1TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Alt1TagEnemyUnitCount"), GS.Alt1TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Alt2TagFriendlyUnitCount"), GS.Alt2TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Alt2TagEnemyUnitCount"), GS.Alt2TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Alt3TagFriendlyUnitCount"), GS.Alt3TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Alt3TagEnemyUnitCount"), GS.Alt3TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Alt4TagFriendlyUnitCount"), GS.Alt4TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Alt4TagEnemyUnitCount"), GS.Alt4TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Alt5TagFriendlyUnitCount"), GS.Alt5TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Alt5TagEnemyUnitCount"), GS.Alt5TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Alt6TagFriendlyUnitCount"), GS.Alt6TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Alt6TagEnemyUnitCount"), GS.Alt6TagEnemyUnitCount);

    SafeSetBBInt(TEXT("Ctrl1TagFriendlyUnitCount"), GS.Ctrl1TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Ctrl1TagEnemyUnitCount"), GS.Ctrl1TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Ctrl2TagFriendlyUnitCount"), GS.Ctrl2TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Ctrl2TagEnemyUnitCount"), GS.Ctrl2TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Ctrl3TagFriendlyUnitCount"), GS.Ctrl3TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Ctrl3TagEnemyUnitCount"), GS.Ctrl3TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Ctrl4TagFriendlyUnitCount"), GS.Ctrl4TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Ctrl4TagEnemyUnitCount"), GS.Ctrl4TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Ctrl5TagFriendlyUnitCount"), GS.Ctrl5TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Ctrl5TagEnemyUnitCount"), GS.Ctrl5TagEnemyUnitCount);
    SafeSetBBInt(TEXT("Ctrl6TagFriendlyUnitCount"), GS.Ctrl6TagFriendlyUnitCount);
    SafeSetBBInt(TEXT("Ctrl6TagEnemyUnitCount"), GS.Ctrl6TagEnemyUnitCount);

    SafeSetBBInt(TEXT("CtrlQTagFriendlyUnitCount"), GS.CtrlQTagFriendlyUnitCount);
    SafeSetBBInt(TEXT("CtrlQTagEnemyUnitCount"), GS.CtrlQTagEnemyUnitCount);
    SafeSetBBInt(TEXT("CtrlWTagFriendlyUnitCount"), GS.CtrlWTagFriendlyUnitCount);
    SafeSetBBInt(TEXT("CtrlWTagEnemyUnitCount"), GS.CtrlWTagEnemyUnitCount);
    SafeSetBBInt(TEXT("CtrlETagFriendlyUnitCount"), GS.CtrlETagFriendlyUnitCount);
    SafeSetBBInt(TEXT("CtrlETagEnemyUnitCount"), GS.CtrlETagEnemyUnitCount);
    SafeSetBBInt(TEXT("CtrlRTagFriendlyUnitCount"), GS.CtrlRTagFriendlyUnitCount);
    SafeSetBBInt(TEXT("CtrlRTagEnemyUnitCount"), GS.CtrlRTagEnemyUnitCount);

    // Periodic debug log (~1s)
    BBPushTickCounter++;
    if (bDebug && (BBPushTickCounter % 10 == 0))
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Pushed BB (TeamId=%d) -> MyUnits=%d EnemyUnits=%d Prim=%.1f/%.1f Rare=%.1f/%.1f AgentPos=(%.0f,%.0f,%.0f)"),
            TeamId, GS.MyUnitCount, GS.EnemyUnitCount, 
            GS.PrimaryResource, GS.MaxPrimaryResource,
            GS.RareResource, GS.MaxRareResource,
            GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z);
    }
}

FString ARTSBTController::ConsumeSelectedActionJSON()
{
    if (SelectedActionJSONKey.IsNone())
    {
        return FString();
    }

    UBlackboardComponent* BB = GetEffectiveBlackboard();
    if (!BB)
    {
        return FString();
    }

    const FString Current = BB->GetValueAsString(SelectedActionJSONKey);

    if (Current.IsEmpty() || Current == LastActionJSON)
    {
        return FString();
    }

    // Clear after consumption so BT must produce a new value
    BlackboardComp->SetValueAsString(SelectedActionJSONKey, TEXT(""));
    LastActionJSON = Current;
    return Current;
}

void ARTSBTController::NotifyBBServiceTick()
{
    LastBBServiceTickTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (bControllerBBFallbackEngaged)
    {
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Received BT Service ping after fallback engaged. Keeping controller-owned updates enabled."));
    }
}

UBlackboardComponent* ARTSBTController::GetEffectiveBlackboard() const
{
    if (BlackboardComp)
    {
        return BlackboardComp;
    }
    if (UBehaviorTreeComponent* BTC = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
    {
        return BTC->GetBlackboardComponent();
    }
    return nullptr;
}

void ARTSBTController::StartControllerBBUpdates()
{
    if (!GetWorld())
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RTSBTController: No World available; cannot start BB updates."));
        return;
    }
    if (!GetEffectiveBlackboard())
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BlackboardComp is null after UseBlackboard; cannot start BB updates."));
        return;
    }

    if (!bEnableControllerBBUpdates)
    {
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Controller-owned BB updates disabled. Expect a BT Service (UBTService_PushGameStateToBB) to feed the Blackboard."));
        return;
    }

    if (bUseTickForBBUpdates)
    {
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Using Tick-driven Blackboard updates every %.3fs (no timer)."), BlackboardUpdateInterval);
        // Tick() will drive updates
    }
    else
    {
        GetWorldTimerManager().SetTimer(BBUpdateTimerHandle, this, &ARTSBTController::PushBlackboardFromGameState_ControllerOwned, BlackboardUpdateInterval, true);
        const bool bActive = GetWorldTimerManager().IsTimerActive(BBUpdateTimerHandle);
        const bool bPaused = GetWorldTimerManager().IsTimerPaused(BBUpdateTimerHandle);
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("RTSBTController: Started controller-owned Blackboard update timer (%.3fs). Active=%s Paused=%s"), BlackboardUpdateInterval, bActive ? TEXT("true") : TEXT("false"), bPaused ? TEXT("true") : TEXT("false"));

        // Defer the first push to the next tick to avoid early GetPawn() races
        FTimerHandle OneShot;
        GetWorldTimerManager().SetTimer(OneShot, FTimerDelegate::CreateLambda([this]() { PushBlackboardFromGameState_ControllerOwned(); }), 0.0f, false);
    }
}
