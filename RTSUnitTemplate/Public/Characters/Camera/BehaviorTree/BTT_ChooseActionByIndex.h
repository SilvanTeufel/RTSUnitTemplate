#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "BTT_ChooseActionByIndex.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UBTT_ChooseActionByIndex : public UBTTask_BlackboardBase
{
    GENERATED_BODY()

public:
    UBTT_ChooseActionByIndex();

    // This is the action to choose
    // This makes the BT configurable from the Details panel!
    UPROPERTY(EditAnywhere, Category = "Action")
    ERTSAIAction Action;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebug = false;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};