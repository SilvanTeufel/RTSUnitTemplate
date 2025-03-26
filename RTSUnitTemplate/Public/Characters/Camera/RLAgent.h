// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "RLAgent.generated.h"

/**
 * ARLAgent is an RL-controlled character that inherits camera and character functionality
 * from AExtendedCameraBase. The RL algorithm provides input values and desired camera states
 * directly to the decision function.
 */

USTRUCT(BlueprintType)
struct FGameStateData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 MyUnitCount = 0;

    UPROPERTY(BlueprintReadWrite)
    int32 EnemyUnitCount = 0;

    UPROPERTY(BlueprintReadWrite)
    float MyTotalHealth = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EnemyTotalHealth = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float MyTotalAttackDamage = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EnemyTotalAttackDamage = 0.0f;
};


UCLASS()
class RTSUNITTEMPLATE_API ARLAgent : public AExtendedCameraBase
{
    GENERATED_BODY()

public:
    // Sets default values for this character
    ARLAgent(const FObjectInitializer& ObjectInitializer);

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Setup input (optional – you might not bind physical input if RL supplies values)
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /**
     * Process RL decision parameters. This function will be called by your external RL algorithm.
     * @param Action - The action chosen by the RL policy (you'll need to define the structure of this array).
     */
    UFUNCTION(BlueprintCallable, Category = "RLAgent|RL")
    void ReceiveRLAction();

private:
    FGameStateData GatherGameState();

    // Add any member variables needed to store the current state or facilitate actions
    TArray<class AUnitBase*> GetMyUnits();
    TArray<class AUnitBase*> GetEnemyUnits();
};