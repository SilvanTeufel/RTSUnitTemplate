#include "Characters/Camera/BehaviorTree/BTT_ChooseAction_RuleBased.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData

UBTT_ChooseAction_RuleBased::UBTT_ChooseAction_RuleBased()
{
	NodeName = TEXT("Choose Action (Rule-Based)");
	BlackboardKey.AddStringFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ChooseAction_RuleBased, BlackboardKey));
}

EBTNodeResult::Type UBTT_ChooseAction_RuleBased::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Missing BlackboardComponent. Ensure your Behavior Tree has a valid Blackboard asset and UseBlackboard was called by an AAIController."));
        return EBTNodeResult::Failed;
    }

    // Prefer AAIController if available, but don't require it. Fallback to AvatarActor (Pawn).
    AAIController* Controller = OwnerComp.GetAIOwner();
    APawn* Pawn = nullptr;
    if (Controller)
    {
        Pawn = Controller->GetPawn();
        if (!Pawn)
        {
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: AAIController present but has no Pawn. Was the pawn possessed?"));
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from AAIController: %s"), *Pawn->GetName());
        }
    }

    if (!Pawn)
    {
        // Fallback: inspect the owner of the behavior tree component
        AActor* OwnerActor = OwnerComp.GetOwner();
        if (OwnerActor)
        {
            // If Owner is any Controller (AI or Player), ask it for its Pawn
            if (AController* AnyController = Cast<AController>(OwnerActor))
            {
                Pawn = AnyController->GetPawn();
                if (Pawn)
                {
                    if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from Controller owner (%s): %s"), *OwnerActor->GetClass()->GetName(), *Pawn->GetName());
                }
                else
                {
                    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Controller owner (%s) has no Pawn (not possessed?)."), *OwnerActor->GetClass()->GetName());
                }
            }
            else
            {
                // In some manual setups the owner itself might be a Pawn
                Pawn = Cast<APawn>(OwnerActor);
                if (Pawn)
                {
                    if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Owner actor cast to Pawn: %s"), *Pawn->GetName());
                }
            }
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: BehaviorTreeComponent has no Owner actor. Cannot resolve Pawn."));
        }
    }

    if (!Pawn && BB && !AgentPawnKey.IsNone())
    {
        if (UObject* Obj = BB->GetValueAsObject(AgentPawnKey))
        {
            Pawn = Cast<APawn>(Obj);
            if (Pawn)
            {
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from BB '%s': %s"), *AgentPawnKey.ToString(), *Pawn->GetName());
            }
        }
    }

    if (!Pawn)
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Failed to resolve Pawn. Aborting task."));
        return EBTNodeResult::Failed;
    }

    URTSRuleBasedDeciderComponent* Decider = Pawn->FindComponentByClass<URTSRuleBasedDeciderComponent>();
    if (!Decider)
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no URTSRuleBasedDeciderComponent."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    FGameStateData GS;

    auto SafeGetBBFloat = [&](FName KeyName) -> float
    {
        if (KeyName.IsNone()) return 0.0f;
        float Val = BB->GetValueAsFloat(KeyName);
        if (Val == 0.0f)
        {
            // Try reading as Int in case the user defined it as Int in Blackboard
            Val = (float)BB->GetValueAsInt(KeyName);
        }
        return Val;
    };

    auto SafeGetBBInt = [&](FName KeyName) -> int32
    {
        if (KeyName.IsNone()) return 0;
        return BB->GetValueAsInt(KeyName);
    };

    GS.MyUnitCount = SafeGetBBInt(MyUnitCountKey);
    GS.EnemyUnitCount = SafeGetBBInt(EnemyUnitCountKey);
    GS.PrimaryResource = SafeGetBBFloat(PrimaryResourceKey);
    GS.SecondaryResource = SafeGetBBFloat(SecondaryResourceKey);
    GS.TertiaryResource = SafeGetBBFloat(TertiaryResourceKey);
    GS.RareResource = SafeGetBBFloat(RareResourceKey);
    GS.EpicResource = SafeGetBBFloat(EpicResourceKey);
    GS.LegendaryResource = SafeGetBBFloat(LegendaryResourceKey);

    GS.MaxPrimaryResource = SafeGetBBFloat(MaxPrimaryResourceKey);
    GS.MaxSecondaryResource = SafeGetBBFloat(MaxSecondaryResourceKey);
    GS.MaxTertiaryResource = SafeGetBBFloat(MaxTertiaryResourceKey);
    GS.MaxRareResource = SafeGetBBFloat(MaxRareResourceKey);
    GS.MaxEpicResource = SafeGetBBFloat(MaxEpicResourceKey);
    GS.MaxLegendaryResource = SafeGetBBFloat(MaxLegendaryResourceKey);

    if (bDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Reading Blackboard Keys:"));
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxPrimaryResourceKey.ToString(), GS.MaxPrimaryResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxSecondaryResourceKey.ToString(), GS.MaxSecondaryResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxTertiaryResourceKey.ToString(), GS.MaxTertiaryResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxRareResourceKey.ToString(), GS.MaxRareResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxEpicResourceKey.ToString(), GS.MaxEpicResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxLegendaryResourceKey.ToString(), GS.MaxLegendaryResource);
    }
    GS.AgentPosition = BB->GetValueAsVector(AgentPositionKey);
    GS.AverageEnemyPosition = BB->GetValueAsVector(AverageEnemyPositionKey);
    GS.AverageFriendlyPosition = BB->GetValueAsVector(TEXT("AverageFriendlyPosition"));

    // Additional totals
    GS.MyTotalHealth = SafeGetBBFloat(TEXT("MyTotalHealth"));
    GS.EnemyTotalHealth = SafeGetBBFloat(TEXT("EnemyTotalHealth"));

    // Populate friendly tag counts
    GS.Alt1TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt1TagFriendlyUnitCount"));
    GS.Alt2TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt2TagFriendlyUnitCount"));
    GS.Alt3TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt3TagFriendlyUnitCount"));
    GS.Alt4TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt4TagFriendlyUnitCount"));
    GS.Alt5TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt5TagFriendlyUnitCount"));
    GS.Alt6TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt6TagFriendlyUnitCount"));

    GS.Ctrl1TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl1TagFriendlyUnitCount"));
    GS.Ctrl2TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl2TagFriendlyUnitCount"));
    GS.Ctrl3TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl3TagFriendlyUnitCount"));
    GS.Ctrl4TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl4TagFriendlyUnitCount"));
    GS.Ctrl5TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl5TagFriendlyUnitCount"));
    GS.Ctrl6TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl6TagFriendlyUnitCount"));

    GS.CtrlQTagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlQTagFriendlyUnitCount"));
    GS.CtrlWTagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlWTagFriendlyUnitCount"));
    GS.CtrlETagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlETagFriendlyUnitCount"));
    GS.CtrlRTagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlRTagFriendlyUnitCount"));

    // Populate enemy tag counts
    GS.Alt1TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt1TagEnemyUnitCount"));
    GS.Alt2TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt2TagEnemyUnitCount"));
    GS.Alt3TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt3TagEnemyUnitCount"));
    GS.Alt4TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt4TagEnemyUnitCount"));
    GS.Alt5TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt5TagEnemyUnitCount"));
    GS.Alt6TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt6TagEnemyUnitCount"));

    GS.Ctrl1TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl1TagEnemyUnitCount"));
    GS.Ctrl2TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl2TagEnemyUnitCount"));
    GS.Ctrl3TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl3TagEnemyUnitCount"));
    GS.Ctrl4TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl4TagEnemyUnitCount"));
    GS.Ctrl5TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl5TagEnemyUnitCount"));
    GS.Ctrl6TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl6TagEnemyUnitCount"));

    GS.CtrlQTagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlQTagEnemyUnitCount"));
    GS.CtrlWTagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlWTagEnemyUnitCount"));
    GS.CtrlETagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlETagEnemyUnitCount"));
    GS.CtrlRTagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlRTagEnemyUnitCount"));

    // Detailed Blackboard snapshot logs (multi-line)
    if (bDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: GS Snapshot -> MyUnits=%d, EnemyUnits=%d, MyHP=%.1f, EnemyHP=%.1f"),
            GS.MyUnitCount, GS.EnemyUnitCount, GS.MyTotalHealth, GS.EnemyTotalHealth);
        UE_LOG(LogTemp, Log, TEXT("  Resources -> Prim=%.2f/%.2f Sec=%.2f/%.2f Ter=%.2f/%.2f Rare=%.2f/%.2f Epic=%.2f/%.2f Leg=%.2f/%.2f"),
            GS.PrimaryResource, GS.MaxPrimaryResource, GS.SecondaryResource, GS.MaxSecondaryResource, GS.TertiaryResource, GS.MaxTertiaryResource,
            GS.RareResource, GS.MaxRareResource, GS.EpicResource, GS.MaxEpicResource, GS.LegendaryResource, GS.MaxLegendaryResource);
        UE_LOG(LogTemp, Log, TEXT("  Positions -> Agent=(%.1f,%.1f,%.1f) AvgFriendly=(%.1f,%.1f,%.1f) AvgEnemy=(%.1f,%.1f,%.1f)"),
            GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z,
            GS.AverageFriendlyPosition.X, GS.AverageFriendlyPosition.Y, GS.AverageFriendlyPosition.Z,
            GS.AverageEnemyPosition.X, GS.AverageEnemyPosition.Y, GS.AverageEnemyPosition.Z);
        UE_LOG(LogTemp, Log, TEXT("  Tags Friendly -> Alt=[%d,%d,%d,%d,%d,%d] Ctrl=[%d,%d,%d,%d,%d,%d] Keys[QWER]=[%d,%d,%d,%d]"),
            GS.Alt1TagFriendlyUnitCount, GS.Alt2TagFriendlyUnitCount, GS.Alt3TagFriendlyUnitCount, GS.Alt4TagFriendlyUnitCount, GS.Alt5TagFriendlyUnitCount, GS.Alt6TagFriendlyUnitCount,
            GS.Ctrl1TagFriendlyUnitCount, GS.Ctrl2TagFriendlyUnitCount, GS.Ctrl3TagFriendlyUnitCount, GS.Ctrl4TagFriendlyUnitCount, GS.Ctrl5TagFriendlyUnitCount, GS.Ctrl6TagFriendlyUnitCount,
            GS.CtrlQTagFriendlyUnitCount, GS.CtrlWTagFriendlyUnitCount, GS.CtrlETagFriendlyUnitCount, GS.CtrlRTagFriendlyUnitCount);
        UE_LOG(LogTemp, Log, TEXT("  Tags Enemy    -> Alt=[%d,%d,%d,%d,%d,%d] Ctrl=[%d,%d,%d,%d,%d,%d] Keys[QWER]=[%d,%d,%d,%d]"),
            GS.Alt1TagEnemyUnitCount, GS.Alt2TagEnemyUnitCount, GS.Alt3TagEnemyUnitCount, GS.Alt4TagEnemyUnitCount, GS.Alt5TagEnemyUnitCount, GS.Alt6TagEnemyUnitCount,
            GS.Ctrl1TagEnemyUnitCount, GS.Ctrl2TagEnemyUnitCount, GS.Ctrl3TagEnemyUnitCount, GS.Ctrl4TagEnemyUnitCount, GS.Ctrl5TagEnemyUnitCount, GS.Ctrl6TagEnemyUnitCount,
            GS.CtrlQTagEnemyUnitCount, GS.CtrlWTagEnemyUnitCount, GS.CtrlETagEnemyUnitCount, GS.CtrlRTagEnemyUnitCount);
    }

    const FString Json = Decider->ChooseJsonActionRuleBased(GS);
    if (Json.IsEmpty())
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Decider returned empty JSON. Check rule thresholds and component configuration on %s."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    // Immediate execution path
    if (bExecuteActionImmediately)
    {
        if (UInferenceComponent* Inference = Pawn->FindComponentByClass<UInferenceComponent>())
        {
            if (bDebug)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Executing action immediately via InferenceComponent (len=%d)."), Json.Len());
            }
            Inference->ExecuteActionFromJSON(Json);
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no UInferenceComponent; cannot execute action immediately."), *Pawn->GetName());
        }

        if (bAlsoWriteToBB)
        {
            BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
            if (bDebug)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Also wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
            }
        }
        return EBTNodeResult::Succeeded;
    }

    // Default: write to BB and let a controller consume it
    BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
    return EBTNodeResult::Succeeded;
}
