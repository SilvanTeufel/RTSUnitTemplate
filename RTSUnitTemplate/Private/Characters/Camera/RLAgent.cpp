// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameModes/UpgradeGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"

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

    FInputActionValue InputActionValue;
    InputActionValue = FInputActionValue(1.0f);
    Input_Tab_Released(InputActionValue, 0);
    // Set up a timer to update the game state every 0.2 seconds (adjust as needed)
    GetWorldTimerManager().SetTimer(RLUpdateTimerHandle, this, &ARLAgent::UpdateGameState, 5.f, true);

}

FString ARLAgent::CreateGameStateJSON(const FGameStateData& GameState)
{
    return FString::Printf(TEXT("{\"MyUnits\": %d, \"EnemyUnits\": %d, \"MyHealth\": %.2f, \"EnemyHealth\": %.2f, \"MyAttack\": %.2f, \"EnemyAttack\": %.2f, \"AgentPosition\": [%.2f, %.2f, %.2f], \"AvgFriendlyPos\": [%.2f, %.2f, %.2f], \"AvgEnemyPos\": [%.2f, %.2f, %.2f], \"PrimaryResource\": %.2f, \"SecondaryResource\": %.2f, \"TertiaryResource\": %.2f, \"RareResource\": %.2f, \"EpicResource\": %.2f, \"LegendaryResource\": %.2f}"),
        GameState.MyUnitCount,
        GameState.EnemyUnitCount,
        GameState.MyTotalHealth,
        GameState.EnemyTotalHealth,
        GameState.MyTotalAttackDamage,
        GameState.EnemyTotalAttackDamage,
        GameState.AgentPosition.X,
        GameState.AgentPosition.Y,
        GameState.AgentPosition.Z,
        GameState.AverageFriendlyPosition.X,
        GameState.AverageFriendlyPosition.Y,
        GameState.AverageFriendlyPosition.Z,
        GameState.AverageEnemyPosition.X,
        GameState.AverageEnemyPosition.Y,
        GameState.AverageEnemyPosition.Z,
        GameState.PrimaryResource,
        GameState.SecondaryResource,
        GameState.TertiaryResource,
        GameState.RareResource,
        GameState.EpicResource,
        GameState.LegendaryResource
    );
}

void ARLAgent::UpdateGameState()
{
    UE_LOG(LogTemp, Log, TEXT("UpdateGameState"));
    FGameStateData GameState = GatherGameState();

    // Convert GameStateData to JSON (or another format your RL process understands)
    FString GameStateJSON = CreateGameStateJSON(GameState);

    // Write to shared memory
    if (SharedMemoryManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Trying to send String: %s"), *GameStateJSON);
        SharedMemoryManager->WriteGameState(GameStateJSON);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SharedMemoryManager is not valid, cannot write game state."));
    }


    // Check for new actions
    CheckForNewActions();
}

void ARLAgent::CheckForNewActions()
{
    UE_LOG(LogTemp, Log, TEXT("CheckForNewActions"));
    FString ActionJSON = SharedMemoryManager->ReadAction();

   // ReceiveRLAction(ActionJSON);
    /*
    if (!ActionJSON.IsEmpty())
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ActionJSON);

        if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
        {
            // Pass the parsed JSON object to ReceiveRLAction
            ReceiveRLAction(JsonObject);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Failed to deserialize Action JSON: %s"), *ActionJSON);
        }
    }*/
}

void ARLAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}


void ARLAgent::ReceiveRLAction(FString ActionJSON)
{
    UE_LOG(LogTemp, Log, TEXT("ReceiveRLAction"));

    if (!ActionJSON.IsEmpty())
    {
        TSharedPtr<FJsonObject> Action;
        TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ActionJSON);

        if (FJsonSerializer::Deserialize(JsonReader, Action) && Action.IsValid())
        {
            if (!Action.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Received invalid Action JSON."));
                return;
            }

            FInputActionValue InputActionValue(1.0f);
            int32 NewCameraState = 0;
            // bool AltIsPressed = false;
            // bool CtrlIsPressed = false;
            FString ActionName = Action->GetStringField(TEXT("action"));

            AExtendedControllerBase* ExtendedController = Cast<AExtendedControllerBase>(GetController());
            if (!ExtendedController)
            {
                UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase."));
                return;
            }

            if (Action->HasField(TEXT("input_value")))
            {
                InputActionValue = FInputActionValue(static_cast<float>(Action->GetNumberField(TEXT("input_value"))));
                UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Input Value to: %.2f"), InputActionValue.Get<float>());
            }
            if (Action->HasField(TEXT("alt")))
            {
                ExtendedController->AltIsPressed = Action->GetBoolField(TEXT("alt"));
                UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Alt to: %s"), ExtendedController->AltIsPressed ? TEXT("True") : TEXT("False"));
            }
            if (Action->HasField(TEXT("ctrl")))
            {
                ExtendedController->IsStrgPressed = Action->GetBoolField(TEXT("ctrl"));
                UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Ctrl to: %s"), ExtendedController->IsStrgPressed ? TEXT("True") : TEXT("False"));
            }
            // ExtendedController->SetModifierKeys(AltIsPressed, CtrlIsPressed);

            if (Action->HasField(TEXT("camera_state")))
            {
                NewCameraState = static_cast<int32>(Action->GetNumberField(TEXT("camera_state")));
                UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Camera State to: %d"), NewCameraState);
            }

            if (ActionName == "switch_camera_state" || ActionName.StartsWith("switch_camera_state_ability") || ActionName.StartsWith("move_camera") || ActionName.StartsWith("stop_move_camera") || ActionName == "change_ability_index")
            {
                SwitchControllerStateMachine(InputActionValue, NewCameraState);
            }
            else if (ActionName == "left_click")
            {
                FVector StartLocation = GetActorLocation() + FVector(0, 0, 500.0f); // Start slightly above
                FVector EndLocationDown = GetActorLocation() - FVector(0, 0, 1000.0f); // Trace downwards
                FHitResult HitResult;
                FCollisionQueryParams CollisionParams;
                CollisionParams.AddIgnoredActor(this); // Ignore this actor

                if (NewCameraState == 1 && GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocationDown, ECC_Visibility, CollisionParams))
                {
                    PerformLeftClickAction(HitResult);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Left Click: No ground hit found."));
                }
        
                if (NewCameraState == 2)
                    ExtendedController->LeftClickReleased();
        

            }
            else if (ActionName == "right_click")
            {
                FVector StartLocation = GetActorLocation() + FVector(0, 0, 500.0f); // Start slightly above
                FVector EndLocationDown = GetActorLocation() - FVector(0, 0, 1000.0f); // Trace downwards
                FHitResult HitResult;
                FCollisionQueryParams CollisionParams;
                CollisionParams.AddIgnoredActor(this); // Ignore this actor

                if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocationDown, ECC_Visibility, CollisionParams))
                {
                    PerformRightClickAction(HitResult);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Right Click: No ground hit found."));
                }
            }
        }
    }

    
    // Handle the initial setting of InputActionValue, Alt, and Ctrl if needed
    // (They are set before potentially calling other functions)
}

void ARLAgent::PerformRightClickAction(const FHitResult& HitResult)
{
    AExtendedControllerBase* ExtendedController = Cast<AExtendedControllerBase>(GetController());
    if (!ExtendedController)
    {
        UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase in PerformRightClickAction."));
        return;
    }

    ExtendedController->AttackToggled = false;

    if (!ExtendedController->CheckClickOnTransportUnit(HitResult))
    {
        if (!ExtendedController->SelectedUnits.Num() || !ExtendedController->SelectedUnits[0]->CurrentDraggedWorkArea)
        {
            if (!ExtendedController->CheckClickOnWorkArea(HitResult))
            {
                ExtendedController->RunUnitsAndSetWaypoints(HitResult);
            }
        }
    }

    if (ExtendedController->SelectedUnits.Num() && ExtendedController->SelectedUnits[0] && ExtendedController->SelectedUnits[0]->CurrentDraggedWorkArea)
    {
        ExtendedController->DestroyDraggedArea(ExtendedController->SelectedUnits[0]);
    }
}

void ARLAgent::PerformLeftClickAction(const FHitResult& HitResult)
{
    AExtendedControllerBase* ExtendedController = Cast<AExtendedControllerBase>(GetController());
    if (!ExtendedController)
    {
        UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase in PerformLeftClickAction."));
        return;
    }

    ExtendedController->LeftClickIsPressed = true;
    ExtendedController->AbilityArrayIndex = 0;

    if (!ExtendedController->CameraBase || ExtendedController->CameraBase->TabToggled) return;

    if (ExtendedController->AltIsPressed)
    {
        ExtendedController->DestroyWorkArea();
        for (int32 i = 0; i < ExtendedController->SelectedUnits.Num(); i++)
        {
            ExtendedController->CancelAbilitiesIfNoBuilding(ExtendedController->SelectedUnits[i]);
        }
    }
    else if (ExtendedController->AttackToggled)
    {
        ExtendedController->AttackToggled = false;

        int32 NumUnits = ExtendedController->SelectedUnits.Num();
        const int32 GridSize = ExtendedController->ComputeGridSize(NumUnits);
        AWaypoint* BWaypoint = nullptr;

        bool PlayWaypointSound = false;
        bool PlayAttackSound = false;

        for (int32 i = 0; i < ExtendedController->SelectedUnits.Num(); i++)
        {
            if (ExtendedController->SelectedUnits[i] != ExtendedController->CameraUnitWithTag)
            {
                int32 Row = i / GridSize;     // Row index
                int32 Col = i % GridSize;     // Column index

                const FVector RunLocation = HitResult.Location + ExtendedController->CalculateGridOffset(Row, Col);

                if (ExtendedController->SetBuildingWaypoint(RunLocation, ExtendedController->SelectedUnits[i], BWaypoint, PlayWaypointSound))
                {
                    // Do Nothing
                }
                else
                {
                    ExtendedController->DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Silver);
                    ExtendedController->LeftClickAttack(ExtendedController->SelectedUnits[i], RunLocation);
                    PlayAttackSound = true;
                }
            }

            if (ExtendedController->SelectedUnits[i])
                ExtendedController->FireAbilityMouseHit(ExtendedController->SelectedUnits[i], HitResult);
        }

        if (ExtendedController->WaypointSound && PlayWaypointSound)
        {
            UGameplayStatics::PlaySound2D(this, ExtendedController->WaypointSound);
        }

        if (ExtendedController->AttackSound && PlayAttackSound)
        {
            UGameplayStatics::PlaySound2D(this, ExtendedController->AttackSound);
        }
    }
    else
    {
        ExtendedController->DropWorkArea();
        //LeftClickSelect_Implementation();

        AActor* HitActor = HitResult.GetActor();
        bool AbilityFired = false;
        for (int32 i = 0; i < ExtendedController->SelectedUnits.Num(); i++)
        {
            if (ExtendedController->SelectedUnits[i] && ExtendedController->SelectedUnits[i]->CurrentSnapshot.AbilityClass && ExtendedController->SelectedUnits[i]->CurrentDraggedAbilityIndicator)
            {
                ExtendedController->FireAbilityMouseHit(ExtendedController->SelectedUnits[i], HitResult);
                AbilityFired = true;
            }
        }

        if (AbilityFired) return;

        AHUD* CurrentHUD = GetWorld()->GetFirstPlayerController()->GetHUD();
        AHUDBase* HUDBase = Cast<AHUDBase>(CurrentHUD);

        if (HitResult.bBlockingHit && HUDBase)
        {
            if (!HitActor->IsA(ALandscape::StaticClass()))
                ExtendedController->ClickedActor = HitActor;
            else
                ExtendedController->ClickedActor = nullptr;

            AUnitBase* UnitBase = Cast<AUnitBase>(HitActor);
            const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(HitActor);

            if (UnitBase && (UnitBase->TeamId == ExtendedController->SelectableTeamId || ExtendedController->SelectableTeamId == 0) && !SUnit)
            {
                HUDBase->DeselectAllUnits();
                HUDBase->SetUnitSelected(UnitBase);
                ExtendedController->DragUnitBase(UnitBase);


                if (ExtendedController->CameraBase->AutoLockOnSelect)
                    ExtendedController->LockCameraToUnit = true;
            }
            else
            {
                HUDBase->InitialPoint = HUDBase->GetMousePos2D();
                HUDBase->bSelectFriendly = true;
            }
        }
    }

    ExtendedController->LeftClickIsPressed = false; // Reset the pressed state
}
/*
ActionSpace:

First choose all of these Variables:

0. InputActionValue = 0 or 1
1. Set AltIsPressed =  true/false
2. Set IsStrgPressed = true/ false

Then Execute one of these functions:
3. if(AltIsPressed) SwitchControllerStateMachine -> NewCameraState = 21 -26 -> Select Units with Tag Alt1-6
4. if(IsStrgPressed) SwitchControllerStateMachine -> NewCameraState = 18 -30 -> Select Units with Tag Strg1-6, F1-F4 And Q, W, E ,R
5. if(!IsStrgPressed) SwitchControllerStateMachine -> NewCameraState = 21 -26 -> Use Ability 1 - 6 From Array with Current Index
6. if(IsStrgPressed) SwitchControllerStateMachine -> NewCameraState = 13 -> Change AbilityArrayIndex
7. if(!IsStrgPressed) SwitchControllerStateMachine -> NewCameraState = 1, 2, 3, 4 -> Move Camera Pawn +/- x,y
8. if(!IsStrgPressed) SwitchControllerStateMachine -> NewCameraState = 111, 222, 333, 444 -> Stop Move Camera Pawn +/- x,y
9. if(!IsStrgPressed) SwitchControllerStateMachine -> LeftClickPressed(PawnPosition)
10. if(!IsStrgPressed) RightClickPressed(PawnPosition)
*/
/*
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
    
    
}*/

FGameStateData ARLAgent::GatherGameState()
{
    UE_LOG(LogTemp, Log, TEXT("GatherGameState"));
    FGameStateData GameState;
    AGameModeBase* BaseGameMode = GetWorld()->GetAuthGameMode();
    AUpgradeGameMode* GameMode = Cast<AUpgradeGameMode>(BaseGameMode);
    if (!GameMode) return GameState;

    UE_LOG(LogTemp, Log, TEXT("GatherGameState2"));

    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
    if (!CameraControllerBase) return GameState;

    UE_LOG(LogTemp, Log, TEXT("GatherGameState3"));

    // Get Agent Position
    GameState.AgentPosition = GetActorLocation();

    // Calculate Average Unit Positions
    FVector SumFriendlyPositions = FVector::ZeroVector;
    int32 NumFriendlyUnits = 0;
    FVector SumEnemyPositions = FVector::ZeroVector;
    int32 NumEnemyUnits = 0;

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
                    SumFriendlyPositions += MyUnit->GetActorLocation();
                    NumFriendlyUnits++;
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("GatherGameState5"));
                    GameState.EnemyUnitCount++;
                    GameState.EnemyTotalHealth += Attributes->GetHealth();
                    GameState.EnemyTotalAttackDamage += Attributes->GetAttackDamage();
                    SumEnemyPositions += MyUnit->GetActorLocation();
                    NumEnemyUnits++;
                }
            }
        }
    }
    
    // Calculate Averages
    if (NumFriendlyUnits > 0)
    {
        GameState.AverageFriendlyPosition = SumFriendlyPositions / NumFriendlyUnits;
    }
    else
    {
        GameState.AverageFriendlyPosition = FVector::ZeroVector; // Or some other default if no friendly units
    }

    if (NumEnemyUnits > 0)
    {
        GameState.AverageEnemyPosition = SumEnemyPositions / NumEnemyUnits;
    }
    else
    {
        GameState.AverageEnemyPosition = FVector::ZeroVector; // Or some other default if no enemy units
    }

    // Get Resources
    if (GameMode && CameraControllerBase)
    {
        int32 MyTeamId = CameraControllerBase->SelectableTeamId;
        GameState.PrimaryResource = GameMode->GetResource(MyTeamId, EResourceType::Primary);
        GameState.SecondaryResource = GameMode->GetResource(MyTeamId, EResourceType::Secondary);
        GameState.TertiaryResource = GameMode->GetResource(MyTeamId, EResourceType::Tertiary);
        GameState.RareResource = GameMode->GetResource(MyTeamId, EResourceType::Rare);
        GameState.EpicResource = GameMode->GetResource(MyTeamId, EResourceType::Epic);
        GameState.LegendaryResource = GameMode->GetResource(MyTeamId, EResourceType::Legendary);
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

