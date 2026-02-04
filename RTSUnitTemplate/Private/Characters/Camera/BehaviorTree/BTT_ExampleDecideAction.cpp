#include "Characters/Camera/BehaviorTree/BTT_ExampleDecideAction.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Characters/Camera/RL/InferenceComponent.h"

UBTT_ExampleDecideAction::UBTT_ExampleDecideAction()
{
    NodeName = TEXT("Example Decide Action (Advantage)");

    // Ensure this task is used with a String blackboard key (SelectedActionJSON)
    BlackboardKey.AddStringFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ExampleDecideAction, BlackboardKey));
}

EBTNodeResult::Type UBTT_ExampleDecideAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!BB || !Controller)
    {
        return EBTNodeResult::Failed;
    }

    APawn* Pawn = Controller->GetPawn();
    if (!Pawn)
    {
        return EBTNodeResult::Failed;
    }

    // Read blackboard values
    const int32 MyCount = BB->GetValueAsInt(MyUnitCountKey);
    const int32 EnemyCount = BB->GetValueAsInt(EnemyUnitCountKey);

    const int32 Diff = MyCount - EnemyCount;
    const bool bAdvantage = Diff >= AdvantageThreshold;

    const ERTSAIAction ChosenAction = bAdvantage ? ActionIfAdvantage : ActionIfDisadvantage;

    // Get InferenceComponent to translate index -> JSON
    UInferenceComponent* InferenceComp = Pawn->FindComponentByClass<UInferenceComponent>();
    if (!InferenceComp)
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("BTT_ExampleDecideAction: Pawn has no UInferenceComponent."));
        return EBTNodeResult::Failed;
    }

    const FString Json = InferenceComp->GetActionAsJSON((int32)ChosenAction);

    // Write to the selected blackboard key (should be SelectedActionJSON)
    BB->SetValueAsString(GetSelectedBlackboardKey(), Json);

    return EBTNodeResult::Succeeded;
}
