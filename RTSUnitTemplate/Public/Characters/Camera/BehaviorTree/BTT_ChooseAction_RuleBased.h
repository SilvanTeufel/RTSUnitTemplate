#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_ChooseAction_RuleBased.generated.h"

class URTSRuleBasedDeciderComponent;
struct FGameStateData;

/**
 * BT Task that asks URTSRuleBasedDeciderComponent to pick a JSON action based on simple rules
 * and writes it into a Blackboard String key (e.g., SelectedActionJSON).
 *
 * It reads these blackboard keys to build FGameStateData input:
 *  - Int: MyUnitCount, EnemyUnitCount
 *  - Float: PrimaryResource, SecondaryResource, TertiaryResource
 *  - Vector: AgentPosition, AverageEnemyPosition
 */
UCLASS()
class RTSUNITTEMPLATE_API UBTT_ChooseAction_RuleBased : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTT_ChooseAction_RuleBased();

	// Blackboard keys to read state from
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MyUnitCountKey = TEXT("MyUnitCount");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName EnemyUnitCountKey = TEXT("EnemyUnitCount");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName PrimaryResourceKey = TEXT("PrimaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName SecondaryResourceKey = TEXT("SecondaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName TertiaryResourceKey = TEXT("TertiaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName AgentPositionKey = TEXT("AgentPosition");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName AverageEnemyPositionKey = TEXT("AverageEnemyPosition");

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
