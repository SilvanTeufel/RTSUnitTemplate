// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"

#include "Characters/Camera/ExtendedCameraBase.h"
#include "Memory/SharedMemoryManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "RL/InferenceComponent.h"


#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h" // Might be needed for FJsonValue types

#include "RLAgent.generated.h"


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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RLAgent, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInferenceComponent> InferenceComponent;
    
public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;


    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RLAgent)
    bool bIsTraining = true;
    // Setup input (optional – you might not bind physical input if RL supplies values)
    //virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void ReceiveRLAction(FString ActionJSON);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void PerformLeftClickAction(const FHitResult& HitResult, bool AttackToggled);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void PerformRightClickAction(const FHitResult& HitResult);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void RunUnitsAndSetWaypoints(FHitResult Hit, AExtendedControllerBase* ExtendedController);
    
    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void AddWorkerToResource(EResourceType ResourceType, int TeamId);

    UFUNCTION(BlueprintCallable, Category = RLAgent)
    void RemoveWorkerFromResource(EResourceType ResourceType, int TeamId);

    UFUNCTION(Category = RLAgent)
    void AgentInitialization();

    // Called from the client to request game state data.
    UFUNCTION(Server, Reliable)
    void Server_PlayGame(int32 SelectableTeamId);
    
    UFUNCTION(Server, Reliable)
    void Server_RequestGameState(int32 SelectableTeamId);

    // Called on the client to receive the game state data.
    UFUNCTION(Client, Reliable)
    void Client_ReceiveGameState(const FGameStateData& GameState);
    
    // Existing function to gather game state on the server.
    FGameStateData GatherGameState(int32 SelectableTeamId);
    
private:
    
    // Manage shared memory
    UFUNCTION( Category = RLAgent)
    void UpdateGameState();

    UFUNCTION( Category = RLAgent)
    void CheckForNewActions();
    
    
    FString CreateGameStateJSON(const FGameStateData& GameState); // New function
    
    // Toggle to enable/disable using shared memory for RL IO (disabled by default for BT mode)
    UPROPERTY(EditAnywhere, Category = RLAgent)
    bool bEnableSharedMemoryIO = false;

    FSharedMemoryManager* SharedMemoryManager;

    FTimerHandle RLUpdateTimerHandle;

    
    // Add any member variables needed to store the current state or facilitate actions
   // TArray<class AUnitBase*> GetMyUnits();
    //TArray<class AUnitBase*> GetEnemyUnits();

private:
    FTimerHandle MyTimerHandle;
    int32 BlackboardPushTickCounter = 0;
};

