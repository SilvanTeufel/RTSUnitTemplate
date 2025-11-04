#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RTSBTController.generated.h"

// Forward declarations requested by user (these are engine classes)
class UBehaviorTree;
class UBlackboardComponent;
class UBehaviorTreeComponent;
class UBlackboardData;
class UInferenceComponent;

/**
 * ARTSBTController
 * A lightweight AIController that owns and runs a Behavior Tree for the RTS unit camera/agent.
 * - Uses UseBlackboard/RunBehaviorTree to wire AI owner + blackboard correctly
 * - Optionally polls a string blackboard key (SelectedActionJSON) each tick
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ARTSBTController : public AAIController
{
	GENERATED_BODY()

public:
	ARTSBTController();

	// Assign your BT asset in defaults or in the editor (or from a BP derived from this controller)
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> StrategyBehaviorTree = nullptr;

	// Name of the string BB key that BT tasks will write the JSON action to
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	FName SelectedActionJSONKey = TEXT("SelectedActionJSON");

	// Optional: poll the BB each tick to consume actions written by BT tasks
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Helper to fetch then clear the SelectedActionJSON once per tick
	UFUNCTION(BlueprintCallable, Category = "AI")
	FString ConsumeSelectedActionJSON();

private:
	// Cached components owned by the controller
	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> BlackboardComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTreeComponent> BehaviorComp = nullptr;

	// Optional cache to avoid re-processing the same action
	FString LastActionJSON;
};
