#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Camera/RL/InferenceComponent.h" // for FGameStateData and UInferenceComponent
#include "RTSRuleBasedDeciderComponent.generated.h"

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
	// ---------------- Resource rules (trigger when Resource > Threshold AND MyUnitCount < MaxUnitCount) ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Primary")
	bool bEnablePrimaryRule = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Primary", meta=(EditCondition="bEnablePrimaryRule"))
	float PrimaryThreshold = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Primary", meta=(EditCondition="bEnablePrimaryRule"))
	int32 PrimaryMaxMyUnitCount = 999;
	// Two-step output: first do selection, then ability (indices into InferenceComponent ActionSpace, 0..29)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Primary", meta=(EditCondition="bEnablePrimaryRule"))
	int32 PrimarySelectionActionIndex = 10; // default: left_click 1 (select)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Primary", meta=(EditCondition="bEnablePrimaryRule"))
	int32 PrimaryAbilityActionIndex = 10;   // default: use ability 1

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Secondary")
	bool bEnableSecondaryRule = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Secondary", meta=(EditCondition="bEnableSecondaryRule"))
	float SecondaryThreshold = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Secondary", meta=(EditCondition="bEnableSecondaryRule"))
	int32 SecondaryMaxMyUnitCount = 999;
	// Two-step output for Secondary
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Secondary", meta=(EditCondition="bEnableSecondaryRule"))
	int32 SecondarySelectionActionIndex = 11; // left_click 1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Secondary", meta=(EditCondition="bEnableSecondaryRule"))
	int32 SecondaryAbilityActionIndex = 11;   // use ability 2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Tertiary")
	bool bEnableTertiaryRule = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Tertiary", meta=(EditCondition="bEnableTertiaryRule"))
	float TertiaryThreshold = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Tertiary", meta=(EditCondition="bEnableTertiaryRule"))
	int32 TertiaryMaxMyUnitCount = 999;
	// Two-step output for Tertiary
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Tertiary", meta=(EditCondition="bEnableTertiaryRule"))
	int32 TertiarySelectionActionIndex = 12; // left_click 1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Rules|Tertiary", meta=(EditCondition="bEnableTertiaryRule"))
	int32 TertiaryAbilityActionIndex = 12;   // use ability 3

	// ---------------- Wander (small movement) fallback ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bEnableWander = true;
	
	// If true, we try to choose a direction biased toward AverageEnemyPosition using the four directional indices below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander")
	bool bBiasTowardEnemy = true;

	// Directional action indices to use when biasing (configure to match your action mapping)
	// Defaults correspond to ActionSpace indices 17..20 (move_camera 1..4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveUpIndex = 17;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveDownIndex = 18;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveLeftIndex = 19;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Wander|Directional", meta=(EditCondition="bBiasTowardEnemy"))
	int32 MoveRightIndex = 20;

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
	bool ShouldTriggerPrimary(const FGameStateData& GS) const;
	bool ShouldTriggerSecondary(const FGameStateData& GS) const;
	bool ShouldTriggerTertiary(const FGameStateData& GS) const;
	int32 PickWanderActionIndex(const FGameStateData& GS) const;
	UInferenceComponent* GetInferenceComponent() const;

	// Compose multiple action indices into a single JSON string. If multiple indices are given, returns a JSON array string.
	FString BuildCompositeActionJSON(const TArray<int32>& Indices, UInferenceComponent* Inference) const;
};
