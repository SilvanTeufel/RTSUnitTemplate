#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "Characters/Camera/RL/InferenceComponent.h"
#include "BTT_ExampleDecideAction.generated.h"

/**
 * Example BT Task that decides between two actions based on a simple condition
 * (friendly units advantage over enemy units) and writes the chosen action as JSON
 * into a Blackboard String key (defaults to "SelectedActionJSON").
 *
 * This serves as a C++ example node you can drop into your Behavior Tree.
 */
UCLASS()
class RTSUNITTEMPLATE_API UBTT_ExampleDecideAction : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTT_ExampleDecideAction();

	// If (MyUnitCount - EnemyUnitCount) >= AdvantageThreshold, we consider it an advantage
	UPROPERTY(EditAnywhere, Category = "Decision")
	int32 AdvantageThreshold = 1;

	// Action to choose when we have advantage
	UPROPERTY(EditAnywhere, Category = "Decision")
	ERTSAIAction ActionIfAdvantage = ERTSAIAction::LeftClick1;

	// Action to choose when we don't have advantage
	UPROPERTY(EditAnywhere, Category = "Decision")
	ERTSAIAction ActionIfDisadvantage = ERTSAIAction::AssignResource1;

	// Blackboard int keys to read from (must exist on your Blackboard asset)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName MyUnitCountKey = TEXT("MyUnitCount");

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName EnemyUnitCountKey = TEXT("EnemyUnitCount");

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
