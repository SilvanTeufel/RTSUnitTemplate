// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Kismet/GameplayStatics.h"

ARLAgent::ARLAgent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Enable ticking for continuous RL decision processing.
    PrimaryActorTick.bCanEverTick = true;
    SharedMemoryManager = new FSharedMemoryManager(TEXT("UnrealRLSharedMemory"), sizeof(SharedData));
}

void ARLAgent::BeginPlay()
{
    Super::BeginPlay();

    // Optionally, if you want the player's view to start on this RL agent:
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
       PC->SetViewTargetWithBlend(this, 0.5f);
    }

    FString SharedMemoryName = "UnrealRLSharedMemory"; // Make sure this matches your Python script
    SIZE_T MemorySizeNeeded = sizeof(SharedData);
    
    if (SharedMemoryManager)
    {
        UE_LOG(LogTemp, Log, TEXT("[ARLAgent] SharedMemoryManager created successfully with name: %s and size: %lld."), *SharedMemoryName, MemorySizeNeeded);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Failed to create SharedMemoryManager."));
    }

    // Set up a timer to update the game state every 0.2 seconds (adjust as needed)
    GetWorldTimerManager().SetTimer(RLUpdateTimerHandle, this, &ARLAgent::UpdateGameState, 5.f, true);

}

void ARLAgent::UpdateGameState()
{



    
    UE_LOG(LogTemp, Log, TEXT("UpdateGameState"));
    FGameStateData GameState = GatherGameState();

    // Convert GameStateData to JSON (or another format your RL process understands)
    FString GameStateJSON = FString::Printf(TEXT("{\"MyUnits\": %d, \"EnemyUnits\": %d, \"MyHealth\": %.2f, \"EnemyHealth\": %.2f}"),
        GameState.MyUnitCount, GameState.EnemyUnitCount, GameState.MyTotalHealth, GameState.EnemyTotalHealth);

    // Write to shared memory
    SharedMemoryManager->WriteGameState(GameStateJSON);

    // Check for new actions
    CheckForNewActions();
}

void ARLAgent::CheckForNewActions()
{
    UE_LOG(LogTemp, Log, TEXT("CheckForNewActions"));
    FString ActionJSON = SharedMemoryManager->ReadAction();

    if (!ActionJSON.IsEmpty())
    {
        // Process the action (parse the JSON if needed)
        ReceiveRLAction();
    }
}

void ARLAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}



void ARLAgent::ReceiveRLAction()
{

    UE_LOG(LogTemp, Log, TEXT("ReceiveRLAction"));
        // Here is Listed what the Agent should change 

        FInputActionValue InputActionValue;
        // To set the value to 0.0f:
        // Add a float Value 0 or 1 for InputActionValue.
       // InputActionValue = FInputActionValue(0.0f);
        InputActionValue = FInputActionValue(1.0f);

        //Choose a CameraState
         int32 NewCameraState = 0;
        // NewCameraState = 21 -26
        // NewCameraState = 18 -30
        // NewCameraState = 1 & 9 & 10
    

    // Also Set AltIsPressed and StrIsPressed
    
    // We also need to Calculate a MousePosition based on the AllUnitsArray
    // But the LeftClick get the Position from the Mouse.
    // We need to rewrite  void AExtendedControllerBase::LeftClickPressed() for this Purpose to pass a Value
    // Call one of the inherited function to imitate the player

    Input_LeftClick_Pressed(InputActionValue, NewCameraState);
    Input_LeftClick_Released(InputActionValue, NewCameraState);

    SwitchControllerStateMachine(InputActionValue, NewCameraState);
    
    
    
}

FGameStateData ARLAgent::GatherGameState()
{
    UE_LOG(LogTemp, Log, TEXT("GatherGameState"));
    FGameStateData GameState;
    AGameModeBase* BaseGameMode = GetWorld()->GetAuthGameMode();
    ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(BaseGameMode);
    if (!GameMode) return GameState;

    UE_LOG(LogTemp, Log, TEXT("GatherGameState2"));
    
    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
    if (!CameraControllerBase) return GameState;

    UE_LOG(LogTemp, Log, TEXT("GatherGameState3"));
    for (AActor* Unit : GameMode->AllUnits)
    {
        AUnitBase* MyUnit = Cast<AUnitBase>(Unit);

        
        if (MyUnit && MyUnit->IsValidLowLevelFast()) // Ensure the unit is valid
        {
            UAttributeSetBase* Attributes = MyUnit->Attributes;
            if (Attributes && Attributes->IsValidLowLevelFast()) // Ensure attributes are valid
            {
                if (MyUnit->TeamId == CameraControllerBase->SelectableTeamId)
                {
                    UE_LOG(LogTemp, Log, TEXT("GatherGameState4"));
                    GameState.MyUnitCount++;
                    GameState.MyTotalHealth += Attributes->GetHealth();
                    GameState.MyTotalAttackDamage += Attributes->GetAttackDamage();
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("GatherGameState5"));
                    GameState.EnemyUnitCount++;
                    GameState.EnemyTotalHealth += Attributes->GetHealth();
                    GameState.EnemyTotalAttackDamage += Attributes->GetAttackDamage();
                }
            }
        }
    }
    return GameState;
}

TArray<AUnitBase*> ARLAgent::GetMyUnits()
{
    TArray<AUnitBase*> MyUnits;
    AGameModeBase* BaseGameMode = GetWorld()->GetAuthGameMode();
    ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(BaseGameMode);
    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());

    if (GameMode && CameraControllerBase)
    {
        for (AActor* UnitActor : GameMode->AllUnits)
        {
            if (AUnitBase* Unit = Cast<AUnitBase>(UnitActor))
            {
                if (Unit->TeamId == CameraControllerBase->SelectableTeamId)
                {
                    MyUnits.Add(Unit);
                }
            }
        }
    }
    return MyUnits;
}

TArray<AUnitBase*> ARLAgent::GetEnemyUnits()
{
    TArray<AUnitBase*> EnemyUnits;
    AGameModeBase* BaseGameMode = GetWorld()->GetAuthGameMode();
    ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(BaseGameMode);
    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());

    if (GameMode && CameraControllerBase)
    {
        for (AActor* UnitActor : GameMode->AllUnits)
        {
            if (AUnitBase* Unit = Cast<AUnitBase>(UnitActor))
            {
                if (Unit->TeamId != CameraControllerBase->SelectableTeamId)
                {
                    EnemyUnits.Add(Unit);
                }
            }
        }
    }
    return EnemyUnits;
}

