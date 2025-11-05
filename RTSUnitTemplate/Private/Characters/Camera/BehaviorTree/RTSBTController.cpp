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
                    UE_LOG(LogTemp, Log, TEXT("RTSBTController: Behavior Tree started in BeginPlay: %s"), *StrategyBehaviorTree->GetName());
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("RTSBTController: RunBehaviorTree failed in BeginPlay for '%s'"), *GetNameSafe(StrategyBehaviorTree));
                }

                // Do not auto-start controller-owned BB updates unless enabled
                StartControllerBBUpdates();
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("RTSBTController: UseBlackboard failed in BeginPlay. Ensure the BT's Blackboard asset is set."));
            }
        }
    }
}

void ARTSBTController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    UE_LOG(LogTemp, Warning, TEXT("!!!!!!!RTSBTController: OnPossess!!!!"));

    if (!ensure(InPawn))
    {
        return;
    }

    // Optional: ensure the pawn has an inference component (used by your BT task)
    if (!InPawn->FindComponentByClass<UInferenceComponent>())
    {
        UE_LOG(LogTemp, Warning, TEXT("RTSBTController: Possessed pawn has no UInferenceComponent. BT tasks like BTT_ChooseActionByIndex may require it."));
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
                UE_LOG(LogTemp, Error, TEXT("RTSBTController: RunBehaviorTree failed for '%s'"), *StrategyBehaviorTree->GetName());
            }
            BehaviorComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());

            UE_LOG(LogTemp, Log, TEXT("RTSBTController: Behavior Tree started: %s"), *StrategyBehaviorTree->GetName());

            // Start controller-owned Blackboard updates based on mode
            StartControllerBBUpdates();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("RTSBTController: UseBlackboard failed. Ensure the BT's Blackboard asset is set."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("RTSBTController: StrategyBehaviorTree is not assigned."));
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
            UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BT Service watchdog timed out (last=%.3f, now=%.3f, timeout=%.3f). Enabling controller-owned Blackboard updates."), LastBBServiceTickTime, Now, BBServiceWatchdogTimeout);
            StartControllerBBUpdates();
        }
    }

    // Optional dispatch: read and clear SelectedActionJSON
    const FString Action = ConsumeSelectedActionJSON();
    if (!Action.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Consumed SelectedActionJSON: %s"), *Action);

        // Dispatch action to the RLAgent's InferenceComponent to actually execute it
        if (UWorld* World = GetWorld())
        {
            TArray<AActor*> Found;
            UGameplayStatics::GetAllActorsOfClass(World, ARLAgent::StaticClass(), Found);
            if (Found.Num() > 0)
            {
                if (ARLAgent* RLAgent = Cast<ARLAgent>(Found[0]))
                {
                    if (UInferenceComponent* Inf = RLAgent->FindComponentByClass<UInferenceComponent>())
                    {
                        Inf->ExecuteActionFromJSON(Action);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("RTSBTController: RLAgent has no UInferenceComponent; cannot execute action."));
                    }
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
    if (EntryCounter % 10 == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: BB updater tick entered (count=%d)"), EntryCounter);
    }

    // Early return logs (rate-limited per second)
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    if (!BlackboardComp)
    {
        if (Now - LastBBWarnTime > 1.0)
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
            if (Now - LastPawnWarnTime > 1.0)
            {
                UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BB updater early return: Pawn is null (not possessing a pawn yet)."));
                LastPawnWarnTime = Now;
            }
            return;
        }
        else
        {
            // Informative (throttled) but continue without a pawn
            if (Now - LastPawnWarnTime > 1.0)
            {
                UE_LOG(LogTemp, Warning, TEXT("RTSBTController: Proceeding with BB update without a possessed Pawn (bRequirePossessedPawnForBBUpdates=false)."));
                LastPawnWarnTime = Now;
            }
        }
    }

    // Locate a RLAgent in the world to gather game state (your existing GatherGameState lives there)
    ARLAgent* RLAgent = nullptr;
    if (UWorld* World = GetWorld())
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(World, ARLAgent::StaticClass(), Found);
        if (Found.Num() > 0)
        {
            RLAgent = Cast<ARLAgent>(Found[0]);
        }
    }
    if (!RLAgent)
    {
        if (Now - LastPawnWarnTime > 1.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BB updater early return: No ARLAgent found in world to provide GameState."));
            LastPawnWarnTime = Now;
        }
        return;
    }

    AExtendedControllerBase* PlayerCtrl = RLAgent->GetController() ? Cast<AExtendedControllerBase>(RLAgent->GetController()) : nullptr;
    if (!PlayerCtrl)
    {
        if (Now - LastPawnWarnTime > 1.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BB updater early return: RLAgent has no AExtendedControllerBase controller (team id unknown)."));
            LastPawnWarnTime = Now;
        }
        return;
    }

    const int32 TeamId = PlayerCtrl->SelectableTeamId;

    // Gather the latest game state via RLAgent's existing helper
    const FGameStateData GS = RLAgent->GatherGameState(TeamId);

    // Write required keys for Rule-Based task
    BlackboardComp->SetValueAsInt(TEXT("MyUnitCount"), GS.MyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("EnemyUnitCount"), GS.EnemyUnitCount);
    BlackboardComp->SetValueAsFloat(TEXT("MyTotalHealth"), GS.MyTotalHealth);
    BlackboardComp->SetValueAsFloat(TEXT("EnemyTotalHealth"), GS.EnemyTotalHealth);
    BlackboardComp->SetValueAsFloat(TEXT("PrimaryResource"), GS.PrimaryResource);
    BlackboardComp->SetValueAsFloat(TEXT("SecondaryResource"), GS.SecondaryResource);
    BlackboardComp->SetValueAsFloat(TEXT("TertiaryResource"), GS.TertiaryResource);
    BlackboardComp->SetValueAsVector(TEXT("AgentPosition"), GS.AgentPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageFriendlyPosition"), GS.AverageFriendlyPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageEnemyPosition"), GS.AverageEnemyPosition);

    // Periodic debug log (~1s)
    BBPushTickCounter++;
    if (BBPushTickCounter % 10 == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Pushed BB -> MyUnits=%d EnemyUnits=%d Prim=%.1f Sec=%.1f Ter=%.1f AgentPos=(%.0f,%.0f,%.0f) AvgEnemy=(%.0f,%.0f,%.0f)"),
            GS.MyUnitCount, GS.EnemyUnitCount, GS.PrimaryResource, GS.SecondaryResource, GS.TertiaryResource,
            GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z,
            GS.AverageEnemyPosition.X, GS.AverageEnemyPosition.Y, GS.AverageEnemyPosition.Z);
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
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Received BT Service ping after fallback engaged. Keeping controller-owned updates enabled."));
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
        UE_LOG(LogTemp, Warning, TEXT("RTSBTController: No World available; cannot start BB updates."));
        return;
    }
    if (!GetEffectiveBlackboard())
    {
        UE_LOG(LogTemp, Warning, TEXT("RTSBTController: BlackboardComp is null after UseBlackboard; cannot start BB updates."));
        return;
    }

    if (!bEnableControllerBBUpdates)
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Controller-owned BB updates disabled. Expect a BT Service (UBTService_PushGameStateToBB) to feed the Blackboard."));
        return;
    }

    if (bUseTickForBBUpdates)
    {
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Using Tick-driven Blackboard updates every %.3fs (no timer)."), BlackboardUpdateInterval);
        // Tick() will drive updates
    }
    else
    {
        GetWorldTimerManager().SetTimer(BBUpdateTimerHandle, this, &ARTSBTController::PushBlackboardFromGameState_ControllerOwned, BlackboardUpdateInterval, true);
        const bool bActive = GetWorldTimerManager().IsTimerActive(BBUpdateTimerHandle);
        const bool bPaused = GetWorldTimerManager().IsTimerPaused(BBUpdateTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("RTSBTController: Started controller-owned Blackboard update timer (%.3fs). Active=%s Paused=%s"), BlackboardUpdateInterval, bActive ? TEXT("true") : TEXT("false"), bPaused ? TEXT("true") : TEXT("false"));

        // Defer the first push to the next tick to avoid early GetPawn() races
        FTimerHandle OneShot;
        GetWorldTimerManager().SetTimer(OneShot, FTimerDelegate::CreateLambda([this]() { PushBlackboardFromGameState_ControllerOwned(); }), 0.0f, false);
    }
}
