// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"

#include "Characters/Camera/ExtendedCameraBase.h"
#include "Memory/SharedMemoryManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"


#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h" // Might be needed for FJsonValue types

#include "RLAgent.generated.h"


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
    //virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void ReceiveRLAction(FString ActionJSON);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void PerformLeftClickAction(const FHitResult& HitResult);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void PerformRightClickAction(const FHitResult& HitResult);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void RunUnitsAndSetWaypoints(FHitResult Hit, AExtendedControllerBase* ExtendedController);
    
    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void AddWorkerToResource(EResourceType ResourceType, int TeamId);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void RemoveWorkerFromResource(EResourceType ResourceType, int TeamId);
    
private:
    FGameStateData GatherGameState();

    // Manage shared memory
    void UpdateGameState();
    void CheckForNewActions();
    FString CreateGameStateJSON(const FGameStateData& GameState); // New function
    
    FSharedMemoryManager* SharedMemoryManager;

    FTimerHandle RLUpdateTimerHandle;

    
    // Add any member variables needed to store the current state or facilitate actions
    TArray<class AUnitBase*> GetMyUnits();
    TArray<class AUnitBase*> GetEnemyUnits();

private:
    FTimerHandle MyTimerHandle;
};

