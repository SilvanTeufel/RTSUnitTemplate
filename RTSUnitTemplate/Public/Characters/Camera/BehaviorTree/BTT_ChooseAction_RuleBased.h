#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTT_ChooseAction_RuleBased.generated.h"

class URTSRuleBasedDeciderComponent;
struct FGameStateData;

/**
 * BT Task that asks URTSRuleBasedDeciderComponent to pick a JSON action based on simple rules
 * and writes it into a Blackboard String key (e.g., SelectedActionJSON).
 * Optionally, it can execute the action immediately via the Pawn's UInferenceComponent.
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
	FName RareResourceKey = TEXT("RareResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName EpicResourceKey = TEXT("EpicResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName LegendaryResourceKey = TEXT("LegendaryResource");

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MaxPrimaryResourceKey = TEXT("MaxPrimaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MaxSecondaryResourceKey = TEXT("MaxSecondaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MaxTertiaryResourceKey = TEXT("MaxTertiaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MaxRareResourceKey = TEXT("MaxRareResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MaxEpicResourceKey = TEXT("MaxEpicResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName MaxLegendaryResourceKey = TEXT("MaxLegendaryResource");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName AgentPositionKey = TEXT("AgentPosition");
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName AverageEnemyPositionKey = TEXT("AverageEnemyPosition");

	// Optional: when the BT runs on an AIController without a pawn, resolve the pawn via this BB Object key
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FName AgentPawnKey = TEXT("AgentPawn");

	// --- Immediate execution options ---
	// If true, the task will execute the chosen action immediately on the Pawn's UInferenceComponent
	// by calling ExecuteActionFromJSON(Json). Useful when no controller-side dispatcher is present.
	UPROPERTY(EditAnywhere, Category="Execution")
	bool bExecuteActionImmediately = true;

	// If true, the task will still write the JSON into the Blackboard key.
	// Disable to avoid double execution when a controller also consumes the BB key.
	UPROPERTY(EditAnywhere, Category="Execution", meta=(EditCondition="bExecuteActionImmediately"))
	bool bAlsoWriteToBB = false;

	// Extra logs around execution
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDebug = false;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
