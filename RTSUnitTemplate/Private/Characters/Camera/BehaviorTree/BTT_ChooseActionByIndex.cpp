#include "Characters/Camera/BehaviorTree/BTT_ChooseActionByIndex.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "GameFramework/Pawn.h"

UBTT_ChooseActionByIndex::UBTT_ChooseActionByIndex()
{
    NodeName = TEXT("Choose RL Action By Index");

    // We want this task to write to a String key (our "SelectedActionJSON")
    BlackboardKey.AddStringFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ChooseActionByIndex, BlackboardKey));
}

EBTNodeResult::Type UBTT_ChooseActionByIndex::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!BlackboardComp || !Controller) return EBTNodeResult::Failed;

    APawn* Pawn = Controller->GetPawn();
    if (!Pawn) return EBTNodeResult::Failed;

    // Get the InferenceComponent from the Pawn
    UInferenceComponent* InferenceComp = Pawn->FindComponentByClass<UInferenceComponent>();
    if (!InferenceComp)
    {
        UE_LOG(LogTemp, Error, TEXT("BTT_ChooseActionByIndex: Could not find InferenceComponent on Pawn."));
        return EBTNodeResult::Failed;
    }

    // Use the component's public function to get the JSON for the specified index
    FString JsonString = InferenceComp->GetActionAsJSON((int32)Action);

    // Write the resulting JSON into the Blackboard key specified in the BT
    BlackboardComp->SetValueAsString(GetSelectedBlackboardKey(), JsonString);

    return EBTNodeResult::Succeeded;
}
