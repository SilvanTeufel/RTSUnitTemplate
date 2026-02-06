#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "Engine/DataTable.h"
#include "Core/UnitData.h" // for FBuildingCost
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData and UInferenceComponent
#include "RTSRuleBasedDeciderComponent.generated.h"

UENUM(BlueprintType)
enum class ERTSUnitCapLogic : uint8
{
	AndLogic UMETA(DisplayName = "AND (All must match)"),
	OrLogic  UMETA(DisplayName = "OR (At least one must match)")
};

USTRUCT(BlueprintType)
struct FRTSUnitCountCap
{
	GENERATED_BODY()

	// The tag to check (e.g., "Alt1", "CtrlQ")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	ERTSUnitTag Tag = ERTSUnitTag::Alt1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	int32 MinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	int32 MaxCount = 999;
};

USTRUCT(BlueprintType)
struct FRTSGameTimeCap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule")
	float Max = 9000.f;
};

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
	FBuildingCost ResourceThresholds;

	// Caps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 MaxFriendlyUnitCount = 999;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	int32 MinFriendlyUnitCount = 0;

	// How to connect the UnitCaps in this row
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	ERTSUnitCapLogic UnitCapLogic = ERTSUnitCapLogic::AndLogic;

	// Per-tag caps for friendly unit counts. If a tag is not in this array, it's not checked (Min=0, Max=999).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	TArray<FRTSUnitCountCap> UnitCaps;

	// Game time constraints (in seconds). Only execute if Min <= GameTime <= Max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	FRTSGameTimeCap GameTimeCap;
	
	// The frequency of this rule (0-100). Higher values relative to other matching rules increase the chance of selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule", meta=(ClampMin="0.0", ClampMax="100.0"))
	float Frequency = 100.0f;

	// Output actions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	ERTSAIAction SelectionAction = ERTSAIAction::Ability1;

	// Optional action between selection and ability (e.g., ChangeAbilityIndex). Ignored if None.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	ERTSAIAction IntermediateAction = ERTSAIAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	ERTSAIAction AbilityAction = ERTSAIAction::Ability1;
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

	// How to connect the UnitCaps in this row. 
	// Default to OR to maintain current aggressive behavior, or AND for coordinated strikes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	ERTSUnitCapLogic UnitCapLogic = ERTSUnitCapLogic::OrLogic;

	// Per-tag caps for friendly unit counts. If a tag is not in this array, it's not checked (Min=0, Max=999).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	TArray<FRTSUnitCountCap> UnitCaps;

	// Game time constraints (in seconds). Only execute if Min <= GameTime <= Max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Caps")
	FRTSGameTimeCap GameTimeCap;

	// The frequency of this rule (0-100). Higher values relative to other matching rules increase the chance of selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule", meta=(ClampMin="0.0", ClampMax="100.0"))
	float Frequency = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	FVector AttackPosition = FVector::ZeroVector;

	// If provided, we will search for actors of these classes and pick one randomly as the attack position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	TArray<TSubclassOf<AActor>> AttackPositionSourceClasses;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rule|Output")
	bool UseClassAttackPositions = false;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;

protected:
	virtual void BeginPlay() override;

public:
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
	// Minimum time between attack rule evaluations. If not elapsed, attack rules are skipped and normal rules/wander run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable", meta=(ClampMin="0.0"))
	float AttackRuleCheckIntervalSeconds = 30.0f;
	// Optional override positions for attack rules by table row index. Index 0 -> Row 0, 1 -> Row 1, etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	TArray<FVector> AttackPositions;

	// Delay in seconds after game start before initial attack position search.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	float AttackPositionUpdateDelay = 2.0f;

	// Interval in seconds between refreshing AttackPositions from classes during runtime. If <= 0.0, no repeating timer is used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|AttackTable")
	float AttackPositionRefreshInterval = 10.0f;

	/** The extent used when projecting a point to the NavMesh to validate attack/move commands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Rules|AttackTable")
	FVector NavMeshProjectionExtent = FVector(50.f, 50.f, 250.f);

	// ---------------- Wander (small movement) fallback ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bEnableWander = true;
	
	// If true, we try to choose a direction biased toward AverageEnemyPosition using the four directional indices below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bBiasTowardEnemy = true;

	// Directional action indices to use when biasing (configure to match your action mapping)
	// Defaults correspond to ActionSpace indices 17..20 (move_camera 1..4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveUpAction = ERTSAIAction::MoveDirection3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveDownAction = ERTSAIAction::MoveDirection4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveLeftAction = ERTSAIAction::MoveDirection2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	ERTSAIAction MoveRightAction = ERTSAIAction::MoveDirection1;

	// If no bias, pick randomly from this set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	TArray<ERTSAIAction> RandomWanderActions;

	// Minimal distance to consider we have a meaningful direction to the enemy (units)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	float EnemyBiasMinDistance = 50.f;

	// Optional two-step wander: do a selection first (e.g., left_click 1) then perform the wander movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bWanderTwoStep = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(EditCondition="bWanderTwoStep"))
	ERTSAIAction WanderSelectionAction = ERTSAIAction::LeftClick1; // left_click 1 by default
	// If you want a specific ability after wander selection, set this to 10..15. If left at None, the second action is the movement itself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(EditCondition="bWanderTwoStep"))
	ERTSAIAction WanderAbilityAction = ERTSAIAction::None;
	// Minimum number of consecutive wander moves to keep in the same direction before switching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander", meta=(ClampMin="1"))
	int32 WanderMinSameDirectionRepeats = 3;

private:
	int32 PickWanderActionIndex(const FGameStateData& GS) const;
	UInferenceComponent* GetInferenceComponent() const;
	// Tracking for wander direction repetition
	int32 LastWanderActionIndex = INDEX_NONE;
	int32 WanderActionRepeatCount = 0;

	// Returns the maximum among all friendly tag unit counts contained in GS
	int32 GetMaxFriendlyTagUnitCount(const FGameStateData& GS) const;

	// Evaluate a single DataTable rule row. Returns empty string if not matched.
	FString EvaluateRuleRow(const FRTSRuleRow& Row, const FGameStateData& GS, UInferenceComponent* Inference) const;

	// If a RulesDataTable is set, iterate rows and return the first matching rule's JSON
	FString EvaluateRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference) const;

	// Evaluates attack rules and executes them if conditions are met.
	bool EvaluateAttackRulesFromDataTable(const FGameStateData& GS, UInferenceComponent* Inference);
	bool ExecuteAttackRuleRow(const FRTSAttackRuleRow& Row, int32 TableRowIndex, const FGameStateData& GS, UInferenceComponent* Inference);

	// Refreshes the cached AttackPositions by searching for actors of classes specified in the DataTable rows.
	void PopulateAttackPositions();

	// Compose multiple action indices into a single JSON string. If multiple indices are given, returns a JSON array string.
	FString BuildCompositeActionJSON(const TArray<int32>& Indices, UInferenceComponent* Inference) const;

	// Timestamp of the last time we attempted to evaluate attack rules (seconds). Initialized so first check is allowed immediately.
	float LastAttackRuleCheckTimeSeconds = -1000000.f;

	FTimerHandle AttackPositionRefreshTimerHandle;

	// While true, block ChooseJsonActionRuleBased from returning any action until the return timer completes
	bool bAttackReturnBlockActive = false;
	// Absolute time (GetWorld()->GetTimeSeconds) when the block should auto-expire (safety in case the timer is canceled)
	float AttackReturnBlockUntilTimeSeconds = 0.f;

	// Location to return the RLAgent to after the attack sequence finishes
	FVector AttackReturnLocation = FVector::ZeroVector;

	// Helper to handle the return move and post-return actions
	void FinalizeAttackReturn();
};
