#include "Characters/Camera/BehaviorTree/RTSBTController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Characters/Camera/RL/InferenceComponent.h"

ARTSBTController::ARTSBTController()
{
    PrimaryActorTick.bCanEverTick = true; // Enable if you want to poll BB each frame
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
    if (BehaviorComp)
    {
        BehaviorComp->StopTree(EBTStopMode::Safe);
    }

    Super::OnUnPossess();
}

void ARTSBTController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Optional dispatch: read and clear SelectedActionJSON
    const FString Action = ConsumeSelectedActionJSON();
    if (!Action.IsEmpty())
    {
        // TODO: Route/execute JSON action in your gameplay logic as needed
        // UE_LOG(LogTemp, Log, TEXT("RTSBTController: Consumed action: %s"), *Action);
    }
}

FString ARTSBTController::ConsumeSelectedActionJSON()
{
    if (!BlackboardComp || SelectedActionJSONKey.IsNone())
    {
        return FString();
    }

    const FString Current = BlackboardComp->GetValueAsString(SelectedActionJSONKey);

    if (Current.IsEmpty() || Current == LastActionJSON)
    {
        return FString();
    }

    // Clear after consumption so BT must produce a new value
    BlackboardComp->SetValueAsString(SelectedActionJSONKey, TEXT(""));
    LastActionJSON = Current;
    return Current;
}
