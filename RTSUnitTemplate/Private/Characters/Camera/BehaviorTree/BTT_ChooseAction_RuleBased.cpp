#include "Characters/Camera/BehaviorTree/BTT_ChooseAction_RuleBased.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
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
    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!BB || !Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Missing Blackboard (%p) or AIController (%p). Ensure UseBlackboard/RunBehaviorTree was called and the BT is owned by an AAIController."), BB, Controller);
        return EBTNodeResult::Failed;
    }

    APawn* Pawn = Controller->GetPawn();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: AIController has no Pawn. Was the pawn possessed?"));
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
    GS.AgentPosition = BB->GetValueAsVector(AgentPositionKey);
    GS.AverageEnemyPosition = BB->GetValueAsVector(AverageEnemyPositionKey);

    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: GS Snapshot -> MyUnits=%d, EnemyUnits=%d, Prim=%.2f, Sec=%.2f, Ter=%.2f, AgentPos=(%.1f,%.1f,%.1f), AvgEnemy=(%.1f,%.1f,%.1f)"),
        GS.MyUnitCount, GS.EnemyUnitCount, GS.PrimaryResource, GS.SecondaryResource, GS.TertiaryResource,
        GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z,
        GS.AverageEnemyPosition.X, GS.AverageEnemyPosition.Y, GS.AverageEnemyPosition.Z);

    const FString Json = Decider->ChooseJsonActionRuleBased(GS);
    if (Json.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Decider returned empty JSON. Check rule thresholds and component configuration on %s."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
    UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
    return EBTNodeResult::Succeeded;
}
