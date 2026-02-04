#include "Characters/Camera/BehaviorTree/BTService_PushGameStateToBB.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData
#include "Characters/Camera/BehaviorTree/RTSBTController.h"
#include "HAL/IConsoleManager.h"

// Console variable to force a TeamId at runtime (for debugging / setups without controller possession)
static TAutoConsoleVariable<int32> CVarRTSBTForcedTeamId(
	TEXT("r.RTSBT.ForcedTeamId"),
	-1,
	TEXT("If >= 0, forces the TeamId used by UBTService_PushGameStateToBB. Useful when RLAgent has no controller yet."),
	ECVF_Default);

UBTService_PushGameStateToBB::UBTService_PushGameStateToBB()
{
	NodeName = TEXT("Push GameState To Blackboard");
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;

	// IMPORTANT: ensure a tick happens even if the branch re-activates often
	bCallTickOnSearchStart = true;          // tick once immediately on activation
	bRestartTimerOnEachActivation = false;  // keep the interval timer across activations

	Interval = 0.1f;        // or 0.05f while testing
	RandomDeviation = 0.0f;
}

void UBTService_PushGameStateToBB::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTService_PushGameStateToBB: BecomeRelevant on %s (owner=%s) Interval=%.3f RandDev=%.3f bCallTickOnSearchStart=%s bRestartTimerOnEachActivation=%s"),
		*GetNameSafe(OwnerComp.GetCurrentTree()), *GetNameSafe(OwnerComp.GetOwner()),
		Interval, RandomDeviation, bCallTickOnSearchStart ? TEXT("true") : TEXT("false"), bRestartTimerOnEachActivation ? TEXT("true") : TEXT("false"));
	// Do an immediate push so we get data even if the branch deactivates quickly
	PushOnce(OwnerComp);
}

void UBTService_PushGameStateToBB::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
	if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTService_PushGameStateToBB: CeaseRelevant on %s (owner=%s)"),
		*GetNameSafe(OwnerComp.GetCurrentTree()), *GetNameSafe(OwnerComp.GetOwner()));
}

void UBTService_PushGameStateToBB::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	TickCounter++;
	if (bDebug && TickCounter % 10 == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("BTService_PushGameStateToBB: TickNode entered (count=%d)"), TickCounter);
	}

	PushOnce(OwnerComp);
}

void UBTService_PushGameStateToBB::PushOnce(UBehaviorTreeComponent& OwnerComp)
{
	UWorld* World = OwnerComp.GetWorld();
	if (!World)
	{
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Early return, World is null."));
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Early return, BlackboardComponent is null. Ensure UseBlackboard was called and a valid Blackboard asset is set on the BT."));
		return;
	}

	// Resolve TeamId in a robust order: Owner Controller -> Forced -> Blackboard -> RLAgent's controller -> World fallback
	int32 TeamId = -1;

	if (AController* OwnerController = Cast<AController>(OwnerComp.GetOwner()))
	{
		if (ARTSBTController* RTSBT = Cast<ARTSBTController>(OwnerController))
		{
			TeamId = RTSBT->OrchestratorTeamId;
		}
		else if (AExtendedControllerBase* ExtPC = Cast<AExtendedControllerBase>(OwnerController))
		{
			TeamId = ExtPC->SelectableTeamId;
		}
	}

	if (TeamId < 0 && ForcedTeamId >= 0)
	{
		TeamId = ForcedTeamId;
	}
	else if (TeamId < 0 && bAllowConsoleForcedTeamId)
	{
		const int32 CVarTeam = CVarRTSBTForcedTeamId.GetValueOnAnyThread();
		if (CVarTeam >= 0)
		{
			TeamId = CVarTeam;
			if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTService_PushGameStateToBB: Using TeamId from cvar r.RTSBT.ForcedTeamId=%d"), TeamId);
		}
	}

	if (TeamId < 0 && !TeamIdBBKey.IsNone())
	{
		TeamId = BB->GetValueAsInt(TeamIdBBKey);
		if (TeamId < 0)
		{
			if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: TeamIdBBKey '%s' is set but value is %d. Will try other sources."), *TeamIdBBKey.ToString(), TeamId);
		}
	}

	// Find an RLAgent that can provide the aggregated FGameStateData
	ARLAgent* RLAgent = nullptr;
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, ARLAgent::StaticClass(), Found);
		
		for (AActor* Actor : Found)
		{
			if (ARLAgent* Candidate = Cast<ARLAgent>(Actor))
			{
				// If we have a team ID, try to find the agent belonging to that team
				if (TeamId >= 0)
				{
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
		}

		if (!RLAgent && Found.Num() > 0)
		{
			RLAgent = Cast<ARLAgent>(Found[0]);
		}
	}

	if (!RLAgent)
	{
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Early return, no ARLAgent found in world to provide GameState."));
		return;
	}

	// Optionally expose the agent pawn to the Blackboard so BT tasks can fetch it
	if (!AgentPawnKey.IsNone())
	{
		BB->SetValueAsObject(AgentPawnKey, RLAgent);
	}

	if (TeamId < 0)
	{
		AExtendedControllerBase* PlayerCtrl = RLAgent->GetController() ? Cast<AExtendedControllerBase>(RLAgent->GetController()) : nullptr;
		if (PlayerCtrl)
		{
			TeamId = PlayerCtrl->SelectableTeamId;
		}
	}

	if (TeamId < 0 && bAllowWorldFallbackController)
	{
		TArray<AActor*> AllCtrls;
		UGameplayStatics::GetAllActorsOfClass(World, AExtendedControllerBase::StaticClass(), AllCtrls);
		if (AllCtrls.Num() > 0)
		{
			if (AExtendedControllerBase* AnyCtrl = Cast<AExtendedControllerBase>(AllCtrls[0]))
			{
				TeamId = AnyCtrl->SelectableTeamId;
				if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Using world-fallback controller '%s' team id %d."), *GetNameSafe(AnyCtrl), TeamId);
			}
		}
	}

	if (TeamId < 0)
	{
		if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Early return, could not resolve TeamId. Consider setting ForcedTeamId or TeamIdBBKey on the service."));
		return;
	}

	const FGameStateData GS = RLAgent->GatherGameState(TeamId);

	auto SafeSetBBFloat = [&](FName KeyName, float Value)
	{
		if (KeyName.IsNone()) return;
		if (BB->GetKeyID(KeyName) == FBlackboard::InvalidKey)
		{
			if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Key '%s' NOT FOUND in Blackboard!"), *KeyName.ToString());
			return;
		}
		BB->SetValueAsFloat(KeyName, Value);
	};

	auto SafeSetBBInt = [&](FName KeyName, int32 Value)
	{
		if (KeyName.IsNone()) return;
		if (BB->GetKeyID(KeyName) == FBlackboard::InvalidKey)
		{
			if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTService_PushGameStateToBB: Key '%s' NOT FOUND in Blackboard!"), *KeyName.ToString());
			return;
		}
		BB->SetValueAsInt(KeyName, Value);
	};

	SafeSetBBInt(MyUnitCountKey, GS.MyUnitCount);
	SafeSetBBInt(EnemyUnitCountKey, GS.EnemyUnitCount);
	SafeSetBBInt(TeamIdBBKey, TeamId);
	SafeSetBBFloat(MyTotalHealthKey, GS.MyTotalHealth);
	SafeSetBBFloat(EnemyTotalHealthKey, GS.EnemyTotalHealth);
	SafeSetBBFloat(PrimaryResourceKey, GS.PrimaryResource);
	SafeSetBBFloat(SecondaryResourceKey, GS.SecondaryResource);
	SafeSetBBFloat(TertiaryResourceKey, GS.TertiaryResource);
	SafeSetBBFloat(RareResourceKey, GS.RareResource);
	SafeSetBBFloat(EpicResourceKey, GS.EpicResource);
	SafeSetBBFloat(LegendaryResourceKey, GS.LegendaryResource);

	SafeSetBBFloat(MaxPrimaryResourceKey, GS.MaxPrimaryResource);
	SafeSetBBFloat(MaxSecondaryResourceKey, GS.MaxSecondaryResource);
	SafeSetBBFloat(MaxTertiaryResourceKey, GS.MaxTertiaryResource);
	SafeSetBBFloat(MaxRareResourceKey, GS.MaxRareResource);
	SafeSetBBFloat(MaxEpicResourceKey, GS.MaxEpicResource);
	SafeSetBBFloat(MaxLegendaryResourceKey, GS.MaxLegendaryResource);
	
	if (!AgentPositionKey.IsNone()) BB->SetValueAsVector(AgentPositionKey, GS.AgentPosition);
	if (!AverageFriendlyPositionKey.IsNone()) BB->SetValueAsVector(AverageFriendlyPositionKey, GS.AverageFriendlyPosition);
	if (!AverageEnemyPositionKey.IsNone()) BB->SetValueAsVector(AverageEnemyPositionKey, GS.AverageEnemyPosition);

	// Per-tag unit counts (friendly/enemy)
	BB->SetValueAsInt(TEXT("Alt1TagFriendlyUnitCount"), GS.Alt1TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Alt1TagEnemyUnitCount"), GS.Alt1TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Alt2TagFriendlyUnitCount"), GS.Alt2TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Alt2TagEnemyUnitCount"), GS.Alt2TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Alt3TagFriendlyUnitCount"), GS.Alt3TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Alt3TagEnemyUnitCount"), GS.Alt3TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Alt4TagFriendlyUnitCount"), GS.Alt4TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Alt4TagEnemyUnitCount"), GS.Alt4TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Alt5TagFriendlyUnitCount"), GS.Alt5TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Alt5TagEnemyUnitCount"), GS.Alt5TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Alt6TagFriendlyUnitCount"), GS.Alt6TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Alt6TagEnemyUnitCount"), GS.Alt6TagEnemyUnitCount);

	BB->SetValueAsInt(TEXT("Ctrl1TagFriendlyUnitCount"), GS.Ctrl1TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl1TagEnemyUnitCount"), GS.Ctrl1TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl2TagFriendlyUnitCount"), GS.Ctrl2TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl2TagEnemyUnitCount"), GS.Ctrl2TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl3TagFriendlyUnitCount"), GS.Ctrl3TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl3TagEnemyUnitCount"), GS.Ctrl3TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl4TagFriendlyUnitCount"), GS.Ctrl4TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl4TagEnemyUnitCount"), GS.Ctrl4TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl5TagFriendlyUnitCount"), GS.Ctrl5TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl5TagEnemyUnitCount"), GS.Ctrl5TagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl6TagFriendlyUnitCount"), GS.Ctrl6TagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("Ctrl6TagEnemyUnitCount"), GS.Ctrl6TagEnemyUnitCount);

	BB->SetValueAsInt(TEXT("CtrlQTagFriendlyUnitCount"), GS.CtrlQTagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlQTagEnemyUnitCount"), GS.CtrlQTagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlWTagFriendlyUnitCount"), GS.CtrlWTagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlWTagEnemyUnitCount"), GS.CtrlWTagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlETagFriendlyUnitCount"), GS.CtrlETagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlETagEnemyUnitCount"), GS.CtrlETagEnemyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlRTagFriendlyUnitCount"), GS.CtrlRTagFriendlyUnitCount);
	BB->SetValueAsInt(TEXT("CtrlRTagEnemyUnitCount"), GS.CtrlRTagEnemyUnitCount);

	const double Now = World->GetTimeSeconds();
 if (bDebug && (Now - LastDebugPrintTime >= DebugPrintInterval))
 {
     LastDebugPrintTime = Now;
     UE_LOG(LogTemp, Log, TEXT("BTService_PushGameStateToBB: Pushed BB (TeamId=%d) -> MyUnits=%d EnemyUnits=%d MyHP=%.1f EnemyHP=%.1f"),
         TeamId, GS.MyUnitCount, GS.EnemyUnitCount, GS.MyTotalHealth, GS.EnemyTotalHealth);
     UE_LOG(LogTemp, Log, TEXT("  Resources -> Prim=%.1f/%.1f Sec=%.1f/%.1f Ter=%.1f/%.1f Rare=%.1f/%.1f Epic=%.1f/%.1f Leg=%.1f/%.1f"),
         GS.PrimaryResource, GS.MaxPrimaryResource, GS.SecondaryResource, GS.MaxSecondaryResource, GS.TertiaryResource, GS.MaxTertiaryResource,
         GS.RareResource, GS.MaxRareResource, GS.EpicResource, GS.MaxEpicResource, GS.LegendaryResource, GS.MaxLegendaryResource);
     UE_LOG(LogTemp, Log, TEXT("  Positions -> Agent=(%.0f,%.0f,%.0f) AvgFriendly=(%.0f,%.0f,%.0f) AvgEnemy=(%.0f,%.0f,%.0f)"),
         GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z,
         GS.AverageFriendlyPosition.X, GS.AverageFriendlyPosition.Y, GS.AverageFriendlyPosition.Z,
         GS.AverageEnemyPosition.X, GS.AverageEnemyPosition.Y, GS.AverageEnemyPosition.Z);
     UE_LOG(LogTemp, Verbose, TEXT("  Tags Friendly -> Alt=[%d,%d,%d,%d,%d,%d] Ctrl=[%d,%d,%d,%d,%d,%d] Keys[QWER]=[%d,%d,%d,%d]"),
         GS.Alt1TagFriendlyUnitCount, GS.Alt2TagFriendlyUnitCount, GS.Alt3TagFriendlyUnitCount, GS.Alt4TagFriendlyUnitCount, GS.Alt5TagFriendlyUnitCount, GS.Alt6TagFriendlyUnitCount,
         GS.Ctrl1TagFriendlyUnitCount, GS.Ctrl2TagFriendlyUnitCount, GS.Ctrl3TagFriendlyUnitCount, GS.Ctrl4TagFriendlyUnitCount, GS.Ctrl5TagFriendlyUnitCount, GS.Ctrl6TagFriendlyUnitCount,
         GS.CtrlQTagFriendlyUnitCount, GS.CtrlWTagFriendlyUnitCount, GS.CtrlETagFriendlyUnitCount, GS.CtrlRTagFriendlyUnitCount);
     UE_LOG(LogTemp, Verbose, TEXT("  Tags Enemy    -> Alt=[%d,%d,%d,%d,%d,%d] Ctrl=[%d,%d,%d,%d,%d,%d] Keys[QWER]=[%d,%d,%d,%d]"),
         GS.Alt1TagEnemyUnitCount, GS.Alt2TagEnemyUnitCount, GS.Alt3TagEnemyUnitCount, GS.Alt4TagEnemyUnitCount, GS.Alt5TagEnemyUnitCount, GS.Alt6TagEnemyUnitCount,
         GS.Ctrl1TagEnemyUnitCount, GS.Ctrl2TagEnemyUnitCount, GS.Ctrl3TagEnemyUnitCount, GS.Ctrl4TagEnemyUnitCount, GS.Ctrl5TagEnemyUnitCount, GS.Ctrl6TagEnemyUnitCount,
         GS.CtrlQTagEnemyUnitCount, GS.CtrlWTagEnemyUnitCount, GS.CtrlETagEnemyUnitCount, GS.CtrlRTagEnemyUnitCount);
 }

	// Ping the controller watchdog so it knows the service is alive
	if (AController* OwnerController = Cast<AController>(OwnerComp.GetOwner()))
	{
		if (ARTSBTController* RTS = Cast<ARTSBTController>(OwnerController))
		{
			RTS->NotifyBBServiceTick();
		}
	}
}
