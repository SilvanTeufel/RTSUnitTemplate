#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData and UInferenceComponent
#include "RTSRuleBasedDeciderComponent.generated.h"

USTRUCT(BlueprintType)
struct FRTSRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	// If false, this row is ignored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	bool bEnabled = true;

	// Optional label for readability in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	FName RuleName;

	// Resource thresholds. All must be met or exceeded (>=) to trigger the rule.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	float PrimaryThreshold = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	float SecondaryThreshold = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	float TertiaryThreshold = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	float RareThreshold = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	float EpicThreshold = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Resources")
	float LegendaryThreshold = 0.f;

	// Caps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 MaxFriendlyUnitCount = 999;

	// Per-tag caps for friendly unit counts (defaults = 999)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt1TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt2TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt3TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt4TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt5TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt6TagMaxFriendlyUnitCount = 999;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl1TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl2TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl3TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl4TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl5TagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl6TagMaxFriendlyUnitCount = 999;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlQTagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlWTagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlETagMaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlRTagMaxFriendlyUnitCount = 999;

	// Output actions (indices into InferenceComponent's ActionSpace)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	int32 SelectionActionIndex = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	int32 AbilityActionIndex = 10;
};


USTRUCT(BlueprintType)
struct FRTSAttackRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	// If false, this row is ignored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	bool bEnabled = true;

	// Optional label for readability in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	FName RuleName;

	// Per-tag caps for friendly unit counts (defaults = 999)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt1TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt2TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt3TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt4TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt5TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Alt6TagMinFriendlyUnitCount = 999;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl1TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl2TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl3TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl4TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl5TagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 Ctrl6TagMinFriendlyUnitCount = 999;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlQTagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlWTagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlETagMinFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps") int32 CtrlRTagMinFriendlyUnitCount = 999;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	FVector AttackPosition = FVector::ZeroVector;
};
/**
 * Easy-to-configure rule-based decider.
 * For each rule you can now output TWO actions in sequence: first a Selection, then an Ability.
 * The function returns a JSON string. When two actions are produced, the string is a JSON array
 * of two action objects: [ {..Selection..}, {..Ability..} ]. Consumers should iterate and execute in order.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API URTSRuleBasedDeciderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSRuleBasedDeciderComponent();

	// Entry point: returns a JSON action string based on simple rules and fallbacks.
	UFUNCTION(BlueprintCallable, Category="AI|Rules")
	FString ChooseJsonActionRuleBased(const FGameStateData& GameState);

public:
	// ---------------- DataTable-based rules (optional) ----------------
	// If true and RulesDataTable is assigned, the component evaluates rows to choose actions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Table")
	bool bUseDataTableRules = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Table", meta=(EditCondition="bUseDataTableRules"))
	UDataTable* RulesDataTable = nullptr;

	// Attack rule DataTable: executes selection + attack at a target position, then returns camera after a delay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	bool bUseAttackDataTableRules = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(EditCondition="bUseAttackDataTableRules"))
	UDataTable* AttackRulesDataTable = nullptr;
	// Time to wait before returning the RLAgent to its original location after issuing attack orders
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0"))
	float AttackReturnDelaySeconds = 3.0f;


	// ---------------- Wander (small movement) fallback ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bEnableWander = true;
	
	// If true, we try to choose a direction biased toward AverageEnemyPosition using the four directional indices below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bBiasTowardEnemy = true;

	// Directional action indices to use when biasing (configure to match your action mapping)
	// Defaults correspond to ActionSpace indices 17..20 (move_camera 1..4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveUpIndex = 19;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveDownIndex = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveLeftIndex = 18;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveRightIndex = 17;

	// If no bias, pick randomly from this set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	TArray<int32> RandomWanderIndices;

	// Minimal distance to consider we have a meaningful direction to the enemy (units)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	float EnemyBiasMinDistance = 50.f;

	// Optional two-step wander: do a selection first (e.g., left_click 1) then perform the wander movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bWanderTwoStep = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(EditCondition="bWanderTwoStep"))
	int32 WanderSelectionActionIndex = 27; // left_click 1 by default
	// If you want a specific ability after wander selection, set this to 10..15. If left at INDEX_NONE, the second action is the movement itself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(EditCondition="bWanderTwoStep"))
	int32 WanderAbilityActionIndex = INDEX_NONE;

private:
	int32 PickWanderActionIndex(const FGameStateData& GS) const;
	UInferenceComponent* GetInferenceComponent() const;

	// Returns the maximum among all friendly tag unit counts contained in GS
	int32 GetMaxFriendlyTagUnitCount(const FGameStateData& GS) const;

	// Evaluate a single DataTable rule row. Returns empty string if not matched.
	FString EvaluateRuleRow(const FRTSRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference) const;

	// If a RulesDataTable is set, iterate rows and return the first matching rule's JSON
	FString EvaluateRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference) const;

	// Attack rules evaluation/execution: returns true if an attack rule executed actions
	bool EvaluateAttackRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference);
	bool ExecuteAttackRuleRow(const FRTSAttackRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference);

	// Compose multiple action indices into a single JSON string. If multiple indices are given, returns a JSON array string.
	FString BuildCompositeActionJSON(const TArray<int32>& Indices, UInferenceComponent* Inference) const;
};
