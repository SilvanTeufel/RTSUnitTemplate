// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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

    /**
     * Die zuletzt gewaehlte Gruppe, die tatsaechlich Einheiten hatte (camera_state der Aktionen 0-9).
     * -1 = noch keine. Gebraucht, weil Gruppenwahl und Faehigkeitsdruck im Aktionsraum getrennt sind:
     * gemessen am 01.09.2026 treffen 61 % der Faehigkeitsdruecke des Netzes eine leere Auswahl,
     * bei der Regel-KI kein einziger - die packt beides in EINE Entscheidung.
     */
    int32 LetzteAuswahlTaste = -1;

    /**
     * Spawn location, used to pick this agent's own "RLAgentCameraBounds" box when the level contains one
     * per team. Taken once at BeginPlay because the agent wanders far from its base later on, and matching
     * against its current position would let it drift into the enemy's box.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RLAgent)
    FVector CameraBoundsReference = FVector::ZeroVector;


public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;


    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RLAgent)
    bool bIsTraining = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RLAgent)
    float DeltaMovement = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RLAgent)
    float FallbackBounceDelta = 1000.0f;

    /**
     * How far from a right-click the agent will re-aim onto a friendly transporter (the Antimatter
     * reactor) when it has loadable workers selected. Its click traces straight down from the camera,
     * so without this it can never hit one and that resource source stays unused. 0 disables.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RLAgent)
    float AiTransporterClickRadius = 3000.0f;

    /**
     * Set from the current action's "aim_at_transporter" field just before the click is executed.
     * Carries the rule's intent to LOAD units, which the agent cannot infer otherwise - the identical
     * right-click is also an ordinary move order for the workers it always has selected.
     */
    bool bActionAimsAtTransporter = false;

    /**
     * Richtet den Angriffsbefehl der KI auf das naechste gegnerische GEBAEUDE statt auf den Boden
     * unter dem Agenten.
     *
     * Ohne das zielt der Angriffsbefehl dorthin, wo die KI-Kamera gerade steht. Fuer die Regel-KI
     * ist das folgenlos - sie geht ueber IssueDirectAttackMove mit eigener Zielwahl. Fuer das NETZ
     * war es der Grund, warum es ueberhaupt nie angreift: es muesste erst die Kamera an den Feind
     * fahren und dann angreifen, und diese Verkettung gelingt ihm praktisch nie. Gemessen ueber
     * 34 Partien: Team 1 (Netz) NULL Angriffsbefehle, Team 2 (Regeln) bis zu 90 - bei besserer
     * Bauleistung des Netzes.
     *
     * Gebaeude als Ziel, weil sie stehen bleiben; dieselbe Begruendung wie bei
     * bPreferBuildingTargets im Regel-Entscheider. Findet sich keines, bleibt es beim
     * urspruenglichen Punkt.
     *
     * Wirkt ausschliesslich im ARLAgent, also nur fuer die KI.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AI")
    bool bAimAttackAtNearestEnemyBuilding = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
    bool bDebug = false;

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

