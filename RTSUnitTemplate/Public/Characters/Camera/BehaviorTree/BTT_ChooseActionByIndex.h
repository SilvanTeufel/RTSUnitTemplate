#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_ChooseActionByIndex.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UBTT_ChooseActionByIndex : public UBTTask_BlackboardBase
{
    GENERATED_BODY()

public:
    UBTT_ChooseActionByIndex();

    // This is the index from the UInferenceComponent::ActionSpace array (0-31)
    // This makes the BT configurable from the Details panel!
    UPROPERTY(EditAnywhere, Category = "Action")
    int32 ActionIndex;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};