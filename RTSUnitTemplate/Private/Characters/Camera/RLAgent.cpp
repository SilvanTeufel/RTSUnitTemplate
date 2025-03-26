// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Kismet/GameplayStatics.h"

ARLAgent::ARLAgent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Enable ticking for continuous RL decision processing.
    PrimaryActorTick.bCanEverTick = true;
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
}

void ARLAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // In a full RL system without the Learning Agents plugin, you would typically:
    // 1. Gather the current game state (this is done in GatherGameState).
    // 2. Send this state to your external RL algorithm.
    // 3. Receive an action back from the RL algorithm.
    // 4. Call ReceiveRLAction with the received action.
    //
    // This communication with the external RL algorithm is something you'll need to implement
    // based on how your RL system is set up (e.g., network communication, file reading, etc.).

    // For now, we'll just gather the state every tick. The actual decision-making
    // and action application will be triggered externally when your RL algorithm provides an action.
    GatherGameState();
}

void ARLAgent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Optionally bind physical input if needed for debugging.
}

/**
 * ReceiveRLAction is called by your external RL algorithm to provide an action.
 * @param Action - The action chosen by the RL policy. You need to define what the elements of this array represent.
 */
void ARLAgent::ReceiveRLAction()
{
    // **YOU NEED TO DEFINE YOUR ACTION SPACE HERE.**
    // This is where you'll interpret the values in the 'Action' array and translate them
    // into commands for your units.


        // Assume the first element of the action array represents the desired NewCameraState.
        //int32 NewCameraState = static_cast<int32>(Action[0]);

        // Create a default FInputActionValue. You might need to adjust this
        // if your RL actions need to simulate specific input values (e.g., for scrolling).

    /* I Think we need a Fitnessfunction for: */

        FInputActionValue InputActionValue;
        // To set the value to 0.0f:
        InputActionValue = FInputActionValue(0.0f);

        // To set the value to 1.0f:
        InputActionValue = FInputActionValue(1.0f);
         int32 NewCameraState = 0;
        // NewCameraState = 21 -26
        // NewCameraState = 18 -30
        // NewCameraState = 1 & 9 & 10
    
        // Add a float Value 0 or 1 for InputActionValue.
// Also Set AltIsPressed and StrIsPressed
    // We also need to Calculate a MousePosition based on the AllUnitsArray
    // But the LeftClick get the Position from the Mouse:

    /*
    void AExtendedControllerBase::LeftClickPressed()
{
    LeftClickIsPressed = true;
    AbilityArrayIndex = 0;
    
    if (!CameraBase || CameraBase->TabToggled) return;
    
    if(AltIsPressed)
    {
        DestroyWorkArea();
        for (int32 i = 0; i < SelectedUnits.Num(); i++)
        {
            CancelAbilitiesIfNoBuilding(SelectedUnits[i]);
        }
        
    }else if (AttackToggled) {
        AttackToggled = false;
        FHitResult Hit;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
...
     **/

    // Call one of the inherited function to imitate the player

    Input_LeftClick_Pressed(InputActionValue, NewCameraState);
    Input_LeftClick_Released(InputActionValue, NewCameraState);

        SwitchControllerStateMachine(InputActionValue, NewCameraState);
    

    

        // You might have more complex actions represented by multiple elements in the 'Action' array.
        // For example:
        // - Action[0]: Unit to control (index or ID)
        // - Action[1]: Action type (move, attack, etc.)
        // - Action[2]: Target X coordinate
        // - Action[3]: Target Y coordinate
        // You'll need to design this based on the complexity of control you want to achieve.
    
}

FGameStateData ARLAgent::GatherGameState()
{
    FGameStateData GameState;
    AGameModeBase* BaseGameMode = GetWorld()->GetAuthGameMode();
    ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(BaseGameMode);
    if (!GameMode) return GameState;

    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
    if (!CameraControllerBase) return GameState;

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
                    GameState.MyUnitCount++;
                    GameState.MyTotalHealth += Attributes->GetHealth();
                    GameState.MyTotalAttackDamage += Attributes->GetAttackDamage();
                }
                else
                {
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