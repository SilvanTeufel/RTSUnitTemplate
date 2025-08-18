// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
};

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

	UFUNCTION(BlueprintCallable, Category = "AI|Inference")
	FString ChooseJsonAction(const FGameStateData& GameState);
protected:
	virtual void BeginPlay() override;

private:
	// Assign your ONNX asset in the editor
	UPROPERTY(EditAnywhere, Category = "AI|Inference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNNEModelData> QNetworkModelData;

	// Runtime model + instance
	TSharedPtr<UE::NNE::IModelCPU> RuntimeModel;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;

	TArray<float> ConvertStateToArray(const FGameStateData& GameStateData) const;
	
	FString GetActionAsJSON(int32 ActionIndex);
	// Helper function to set up the action space
	void InitializeActionSpace();

	TArray<FRLAction> ActionSpace;
};