// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "BehaviorTree/BehaviorTree.h"
#include "InferenceComponent.generated.h"

USTRUCT(BlueprintType)
struct FRLAction
{
	GENERATED_BODY()

	UPROPERTY()
	FString Type;

	UPROPERTY()
	float InputValue;

	UPROPERTY()
	bool bAlt;

	UPROPERTY()
	bool bCtrl;

	UPROPERTY()
	FString Action;

	UPROPERTY()
	int32 CameraState;

	// Default constructor
	FRLAction() : InputValue(0.0f), bAlt(false), bCtrl(false), CameraState(0) {}

	// Helper constructor for easy initialization
	FRLAction(const FString& InType, float InValue, bool InAlt, bool InCtrl, const FString& InAction, int32 InCamState)
		: Type(InType), InputValue(InValue), bAlt(InAlt), bCtrl(InCtrl), Action(InAction), CameraState(InCamState)
	{}
};

class UNNEModelData;
namespace UE::NNE
{
	class IModelCPU;
	class IModelInstanceCPU;
}


USTRUCT(BlueprintType)
struct FGameStateData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	int32 MyUnitCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	int32 EnemyUnitCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float MyTotalHealth = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float EnemyTotalHealth = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float MyTotalAttackDamage = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float EnemyTotalAttackDamage = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	FVector AgentPosition;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	FVector AverageFriendlyPosition;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	FVector AverageEnemyPosition;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float PrimaryResource = 0.0f;
    
	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float SecondaryResource = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float TertiaryResource = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float RareResource = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float EpicResource = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = RLAgent)
	float LegendaryResource = 0.0f;

	// Max Resource caps
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) float MaxPrimaryResource = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) float MaxSecondaryResource = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) float MaxTertiaryResource = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) float MaxRareResource = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) float MaxEpicResource = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) float MaxLegendaryResource = 0.0f;

	// Per-tag unit counts (friendly/enemy) for selection/ability groups
	// Alt1..Alt6
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt1TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt1TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt2TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt2TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt3TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt3TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt4TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt4TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt5TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt5TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt6TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Alt6TagEnemyUnitCount = 0;

	// Ctrl1..Ctrl6
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl1TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl1TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl2TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl2TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl3TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl3TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl4TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl4TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl5TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl5TagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl6TagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 Ctrl6TagEnemyUnitCount = 0;

	// CtrlQ, CtrlW, CtrlE, CtrlR
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlQTagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlQTagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlWTagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlWTagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlETagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlETagEnemyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlRTagFriendlyUnitCount = 0;
	UPROPERTY(BlueprintReadWrite, Category = RLAgent) int32 CtrlRTagEnemyUnitCount = 0;
};

UENUM(BlueprintType)
enum class ERTSAIAction : uint8
{
	// Ctrl + Q, W, E, R
	CtrlR = 0    UMETA(DisplayName = "CTRL+R"),
	CtrlQ = 1    UMETA(DisplayName = "CTRL+Q"),
	CtrlE = 2    UMETA(DisplayName = "CTRL+E"),
	CtrlW = 3    UMETA(DisplayName = "CTRL+W"),

	// Ctrl + 1-6
	Ctrl1 = 4    UMETA(DisplayName = "CTRL+1"),
	Ctrl2 = 5    UMETA(DisplayName = "CTRL+2"),
	Ctrl3 = 6    UMETA(DisplayName = "CTRL+3"),
	Ctrl4 = 7    UMETA(DisplayName = "CTRL+4"),
	Ctrl5 = 8    UMETA(DisplayName = "CTRL+5"),
	Ctrl6 = 9    UMETA(DisplayName = "CTRL+6"),

	// No Modifier + Use Ability 1-6
	Ability1 = 10  UMETA(DisplayName = "Use Ability 1"),
	Ability2 = 11  UMETA(DisplayName = "Use Ability 2"),
	Ability3 = 12  UMETA(DisplayName = "Use Ability 3"),
	Ability4 = 13  UMETA(DisplayName = "Use Ability 4"),
	Ability5 = 14  UMETA(DisplayName = "Use Ability 5"),
	Ability6 = 15  UMETA(DisplayName = "Use Ability 6"),

	// Change Ability Index
	ChangeAbilityIndex = 16 UMETA(DisplayName = "Change Ability Index"),

	// No Modifier + Move Camera
	MoveDirection1 = 17 UMETA(DisplayName = "Move Camera 1"),
	MoveDirection2 = 18 UMETA(DisplayName = "Move Camera 2"),
	MoveDirection3 = 19 UMETA(DisplayName = "Move Camera 3"),
	MoveDirection4 = 20 UMETA(DisplayName = "Move Camera 4"),

	// Set workers to Resource
	AssignResource1 = 21 UMETA(DisplayName = "Assign Workers Resource 1"),
	AssignResource2 = 22 UMETA(DisplayName = "Assign Workers Resource 2"),
	AssignResource3 = 23 UMETA(DisplayName = "Assign Workers Resource 3"),
	AssignResource4 = 24 UMETA(DisplayName = "Assign Workers Resource 4"),
	AssignResource5 = 25 UMETA(DisplayName = "Assign Workers Resource 5"),
	AssignResource6 = 26 UMETA(DisplayName = "Assign Workers Resource 6"),

	// No Modifier + Left Click (Move and Attack)
	LeftClick1 = 27 UMETA(DisplayName = "Left Click 1 (Move)"),
	LeftClick2 = 28 UMETA(DisplayName = "Left Click 2 (Attack)"),

	// No Modifier + Right Click
	RightClick1 = 29 UMETA(DisplayName = "Right Click 1"),

	// Alt + 1-6
	Alt1 = 30 UMETA(DisplayName = "ALT+1"),
	Alt2 = 31 UMETA(DisplayName = "ALT+2"),
	Alt3 = 32 UMETA(DisplayName = "ALT+3"),
	Alt4 = 33 UMETA(DisplayName = "ALT+4"),
	Alt5 = 34 UMETA(DisplayName = "ALT+5"),
	Alt6 = 35 UMETA(DisplayName = "ALT+6"),

	None = 255 UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class ERTSUnitTag : uint8
{
	Alt1 UMETA(DisplayName = "ALT+1"),
	Alt2 UMETA(DisplayName = "ALT+2"),
	Alt3 UMETA(DisplayName = "ALT+3"),
	Alt4 UMETA(DisplayName = "ALT+4"),
	Alt5 UMETA(DisplayName = "ALT+5"),
	Alt6 UMETA(DisplayName = "ALT+6"),

	Ctrl1 UMETA(DisplayName = "CTRL+1"),
	Ctrl2 UMETA(DisplayName = "CTRL+2"),
	Ctrl3 UMETA(DisplayName = "CTRL+3"),
	Ctrl4 UMETA(DisplayName = "CTRL+4"),
	Ctrl5 UMETA(DisplayName = "CTRL+5"),
	Ctrl6 UMETA(DisplayName = "CTRL+6"),

	CtrlQ UMETA(DisplayName = "CTRL+Q"),
	CtrlW UMETA(DisplayName = "CTRL+W"),
	CtrlE UMETA(DisplayName = "CTRL+E"),
	CtrlR UMETA(DisplayName = "CTRL+R"),

	None = 255 UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class EBrainMode : uint8
{
	RL_Model        UMETA(DisplayName = "RL Model (ONNX)"),
	Behavior_Tree   UMETA(DisplayName = "Behavior Tree")
};

class AController;
class UBehaviorTree;
class UBlackboardComponent;
class UBehaviorTreeComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UInferenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	UInferenceComponent();
	virtual ~UInferenceComponent();

	// Takes game state and returns an action index
	UFUNCTION(BlueprintCallable, Category = "AI|Inference")
	int32 ChooseAction(const TArray<float>& GameState);

	// Router that chooses between RL and BT
	UFUNCTION(BlueprintCallable, Category = "AI|Inference")
	FString ChooseJsonAction(const FGameStateData& GameState);

	// Expose for BT task to fetch JSON for an index
	UFUNCTION(BlueprintCallable, Category = "AI|Inference")
	FString GetActionAsJSON(int32 ActionIndex);

	// Dispatch and execute one or more JSON actions (object or array of objects)
	UFUNCTION(BlueprintCallable, Category = "AI|Actions")
	void ExecuteActionFromJSON(const FString& Json);

	// Implement this in Blueprint to actually perform a parsed action (input/camera/ability, etc.)
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|Actions")
	void PerformParsedAction(const FString& Type, const FString& Action, int32 CameraState, float InputValue, bool bAlt, bool bCtrl);

	// Call this from ARLAgent after the controller is set
	void InitializeBehaviorTree(class AController* OwnerController);

// Expose current brain mode to other systems
	EBrainMode GetBrainMode() const { return BrainMode; }

	// Public wrapper to push GameState into the Blackboard (safe to call from controllers/pawns/services)
	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree")
	void PushBlackboardFromGameState(const FGameStateData& GameState);

protected:
	virtual void BeginPlay() override;

	// Update Blackboard values from game state
	void UpdateBlackboard(const FGameStateData& GameState);

protected:
	// Brain routing and BT assets
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	EBrainMode BrainMode = EBrainMode::RL_Model;

	// Require an AIController to own/run BT. When true, manual fallback (NewObject) is disabled.
	UPROPERTY(EditAnywhere, Category = "AI|BehaviorTree")
	bool bRequireAIControllerForBT = true;

	// Assign the BT asset to run when BrainMode is Behavior_Tree
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "BrainMode == EBrainMode::Behavior_Tree"))
	UBehaviorTree* StrategyBehaviorTree = nullptr;

	// Runtime instances of the BT components
	UPROPERTY(Transient)
	UBlackboardComponent* BlackboardComp = nullptr;

	UPROPERTY(Transient)
	UBehaviorTreeComponent* BehaviorTreeComp = nullptr;

private:
	// Assign your ONNX asset in the editor
	UPROPERTY(EditAnywhere, Category = "AI|Inference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNNEModelData> QNetworkModelData;

	// Runtime model + instance
	TSharedPtr<UE::NNE::IModelCPU> RuntimeModel;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;

	TArray<float> ConvertStateToArray(const FGameStateData& GameStateData) const;
	
	// Helper function to set up the action space
	void InitializeActionSpace();

	TArray<FRLAction> ActionSpace;

	// Old behavior (RL) - renamed from ChooseJsonAction
	FString GetActionFromRLModel(const FGameStateData& GameState);
};