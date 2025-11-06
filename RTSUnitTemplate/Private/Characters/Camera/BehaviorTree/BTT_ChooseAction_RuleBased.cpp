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
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Missing BlackboardComponent. Ensure your Behavior Tree has a valid Blackboard asset and UseBlackboard was called by an AAIController."));
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
            UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: AAIController present but has no Pawn. Was the pawn possessed?"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from AAIController: %s"), *Pawn->GetName());
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
                    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from Controller owner (%s): %s"), *OwnerActor->GetClass()->GetName(), *Pawn->GetName());
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Controller owner (%s) has no Pawn (not possessed?)."), *OwnerActor->GetClass()->GetName());
                }
            }
            else
            {
                // In some manual setups the owner itself might be a Pawn
                Pawn = Cast<APawn>(OwnerActor);
                if (Pawn)
                {
                    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Owner actor cast to Pawn: %s"), *Pawn->GetName());
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: BehaviorTreeComponent has no Owner actor. Cannot resolve Pawn."));
        }
    }

    if (!Pawn && BB && !AgentPawnKey.IsNone())
    {
        if (UObject* Obj = BB->GetValueAsObject(AgentPawnKey))
        {
            Pawn = Cast<APawn>(Obj);
            if (Pawn)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from BB '%s': %s"), *AgentPawnKey.ToString(), *Pawn->GetName());
            }
        }
    }

    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Failed to resolve Pawn. Aborting task."));
        return EBTNodeResult::Failed;
    }

    URTSRuleBasedDeciderComponent* Decider = Pawn->FindComponentByClass<URTSRuleBasedDeciderComponent>();
    if (!Decider)
    {
        UE_LOG(LogTemp, Error, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no URTSRuleBasedDeciderComponent."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    FGameStateData GS;
    GS.MyUnitCount = BB->GetValueAsInt(MyUnitCountKey);
    GS.EnemyUnitCount = BB->GetValueAsInt(EnemyUnitCountKey);
    GS.PrimaryResource = BB->GetValueAsFloat(PrimaryResourceKey);
    GS.SecondaryResource = BB->GetValueAsFloat(SecondaryResourceKey);
    GS.TertiaryResource = BB->GetValueAsFloat(TertiaryResourceKey);
    // Additional resources (provided by BT service/component under fixed keys)
    GS.RareResource = BB->GetValueAsFloat(TEXT("RareResource"));
    GS.EpicResource = BB->GetValueAsFloat(TEXT("EpicResource"));
    GS.LegendaryResource = BB->GetValueAsFloat(TEXT("LegendaryResource"));
    GS.AgentPosition = BB->GetValueAsVector(AgentPositionKey);
    GS.AverageEnemyPosition = BB->GetValueAsVector(AverageEnemyPositionKey);
    GS.AverageFriendlyPosition = BB->GetValueAsVector(TEXT("AverageFriendlyPosition"));

    // Additional totals
    GS.MyTotalHealth = BB->GetValueAsFloat(TEXT("MyTotalHealth"));
    GS.EnemyTotalHealth = BB->GetValueAsFloat(TEXT("EnemyTotalHealth"));

    // Populate friendly tag counts
    GS.Alt1TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Alt1TagFriendlyUnitCount"));
    GS.Alt2TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Alt2TagFriendlyUnitCount"));
    GS.Alt3TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Alt3TagFriendlyUnitCount"));
    GS.Alt4TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Alt4TagFriendlyUnitCount"));
    GS.Alt5TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Alt5TagFriendlyUnitCount"));
    GS.Alt6TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Alt6TagFriendlyUnitCount"));

    GS.Ctrl1TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Ctrl1TagFriendlyUnitCount"));
    GS.Ctrl2TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Ctrl2TagFriendlyUnitCount"));
    GS.Ctrl3TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Ctrl3TagFriendlyUnitCount"));
    GS.Ctrl4TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Ctrl4TagFriendlyUnitCount"));
    GS.Ctrl5TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Ctrl5TagFriendlyUnitCount"));
    GS.Ctrl6TagFriendlyUnitCount = BB->GetValueAsInt(TEXT("Ctrl6TagFriendlyUnitCount"));

    GS.CtrlQTagFriendlyUnitCount = BB->GetValueAsInt(TEXT("CtrlQTagFriendlyUnitCount"));
    GS.CtrlWTagFriendlyUnitCount = BB->GetValueAsInt(TEXT("CtrlWTagFriendlyUnitCount"));
    GS.CtrlETagFriendlyUnitCount = BB->GetValueAsInt(TEXT("CtrlETagFriendlyUnitCount"));
    GS.CtrlRTagFriendlyUnitCount = BB->GetValueAsInt(TEXT("CtrlRTagFriendlyUnitCount"));

    // Populate enemy tag counts
    GS.Alt1TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Alt1TagEnemyUnitCount"));
    GS.Alt2TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Alt2TagEnemyUnitCount"));
    GS.Alt3TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Alt3TagEnemyUnitCount"));
    GS.Alt4TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Alt4TagEnemyUnitCount"));
    GS.Alt5TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Alt5TagEnemyUnitCount"));
    GS.Alt6TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Alt6TagEnemyUnitCount"));

    GS.Ctrl1TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Ctrl1TagEnemyUnitCount"));
    GS.Ctrl2TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Ctrl2TagEnemyUnitCount"));
    GS.Ctrl3TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Ctrl3TagEnemyUnitCount"));
    GS.Ctrl4TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Ctrl4TagEnemyUnitCount"));
    GS.Ctrl5TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Ctrl5TagEnemyUnitCount"));
    GS.Ctrl6TagEnemyUnitCount = BB->GetValueAsInt(TEXT("Ctrl6TagEnemyUnitCount"));

    GS.CtrlQTagEnemyUnitCount = BB->GetValueAsInt(TEXT("CtrlQTagEnemyUnitCount"));
    GS.CtrlWTagEnemyUnitCount = BB->GetValueAsInt(TEXT("CtrlWTagEnemyUnitCount"));
    GS.CtrlETagEnemyUnitCount = BB->GetValueAsInt(TEXT("CtrlETagEnemyUnitCount"));
    GS.CtrlRTagEnemyUnitCount = BB->GetValueAsInt(TEXT("CtrlRTagEnemyUnitCount"));

    // Detailed Blackboard snapshot logs (multi-line)
    UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: GS Snapshot -> MyUnits=%d, EnemyUnits=%d, MyHP=%.1f, EnemyHP=%.1f"),
        GS.MyUnitCount, GS.EnemyUnitCount, GS.MyTotalHealth, GS.EnemyTotalHealth);
    UE_LOG(LogTemp, Log, TEXT("  Resources -> Prim=%.2f Sec=%.2f Ter=%.2f Rare=%.2f Epic=%.2f Leg=%.2f"),
        GS.PrimaryResource, GS.SecondaryResource, GS.TertiaryResource, GS.RareResource, GS.EpicResource, GS.LegendaryResource);
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

    const FString Json = Decider->ChooseJsonActionRuleBased(GS);
    if (Json.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Decider returned empty JSON. Check rule thresholds and component configuration on %s."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    // Immediate execution path
    if (bExecuteActionImmediately)
    {
        if (UInferenceComponent* Inference = Pawn->FindComponentByClass<UInferenceComponent>())
        {
            if (bLogExecution)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Executing action immediately via InferenceComponent (len=%d)."), Json.Len());
            }
            Inference->ExecuteActionFromJSON(Json);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no UInferenceComponent; cannot execute action immediately."), *Pawn->GetName());
        }

        if (bAlsoWriteToBB)
        {
            BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
            if (bLogExecution)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Also wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
            }
        }
        return EBTNodeResult::Succeeded;
    }

    // Default: write to BB and let a controller consume it
    BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
    UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
    return EBTNodeResult::Succeeded;
}
