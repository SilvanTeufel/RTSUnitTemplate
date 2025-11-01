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
    InferenceComponent = CreateDefaultSubobject<UInferenceComponent>(TEXT("InferenceComponent"));
}

void ARLAgent::BeginPlay()
{
    Super::BeginPlay();
}


void ARLAgent::AgentInitialization()
{
    FString MemoryName;
 
    if (AExtendedControllerBase* ExtendedController = Cast<AExtendedControllerBase>(GetController()))
    {
        
        ExtendedController->SetViewTargetWithBlend(this, 0.5f);
        
        if (AExtendedControllerBase* ControllerBase = Cast<AExtendedControllerBase>(ExtendedController))
        {
            // Retrieve the Team ID from ControllerBase
            int32 TeamId = ControllerBase->SelectableTeamId;
        
            // Create the memory name using FString::Printf
            MemoryName = FString::Printf(TEXT("UnrealRLSharedMemory_TeamId_%d"), TeamId);

            UE_LOG(LogTemp, Log, TEXT("[ARLAgent] TRYING to create SharedMemoryManager."));
            // Convert FString to TCHAR* for FSharedMemoryManager
            SharedMemoryManager = new FSharedMemoryManager(*MemoryName, sizeof(SharedData));
        }

        // --- Initialize Behavior Tree brain components if enabled ---
        if (InferenceComponent)
        {
            InferenceComponent->InitializeBehaviorTree(ExtendedController);
        }
    }
    
    SIZE_T MemorySizeNeeded = sizeof(SharedData);
    
    if (GetWorld() && GetWorld()->IsNetMode(ENetMode::NM_Client))
    {
        if (SharedMemoryManager)
        {
            UE_LOG(LogTemp, Log, TEXT("[ARLAgent] SharedMemoryManager created successfully with name: %s and size: %lld."), *MemoryName, MemorySizeNeeded);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Failed to create SharedMemoryManager."));
        }
    }

    FInputActionValue InputActionValue;
    InputActionValue = FInputActionValue(1.0f);
    Input_Tab_Released(InputActionValue, 0);
    // Set up a timer to update the game state every 0.2 seconds (adjust as needed)
    GetWorldTimerManager().SetTimer(RLUpdateTimerHandle, this, &ARLAgent::UpdateGameState, 0.1f, true);
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
   // UE_LOG(LogTemp, Log, TEXT("UpdateGameState"));

    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
    if (!CameraControllerBase) return;

    if (bIsTraining)
        Server_RequestGameState(CameraControllerBase->SelectableTeamId);
    else
        Server_PlayGame(CameraControllerBase->SelectableTeamId);
    
}

void ARLAgent::CheckForNewActions()
{
    FString ActionJSON = SharedMemoryManager->ReadAction();

    ReceiveRLAction(ActionJSON);
}

void ARLAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    
    
}


void ARLAgent::ReceiveRLAction(FString ActionJSON)
{

    const FString UTF8_BOM = TEXT("\xEF\xBB\xBF");
    
    if (!ActionJSON.IsEmpty())
    {
        // Check for and remove the BOM character
        FString CleanedActionJSON = ActionJSON.TrimStart();

        if (!CleanedActionJSON.IsEmpty() && CleanedActionJSON[0] == 0xFEFF)
        {
            CleanedActionJSON.RemoveAt(0);
        }
        
        TSharedPtr<FJsonObject> Action;
        TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(CleanedActionJSON);

        if (FJsonSerializer::Deserialize(JsonReader, Action) && Action.IsValid())
        {
            if (!Action.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Received invalid Action JSON."));
                return;
            }

            FInputActionValue InputActionValue(1.0f);
            int32 NewCameraState = 0;
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
                ExtendedController->IsCtrlPressed = Action->GetBoolField(TEXT("ctrl"));
                UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Ctrl to: %s"), ExtendedController->IsCtrlPressed ? TEXT("True") : TEXT("False"));
            }
            // ExtendedController->SetModifierKeys(AltIsPressed, CtrlIsPressed);

            if (Action->HasField(TEXT("camera_state")))
            {
                NewCameraState = static_cast<int32>(Action->GetNumberField(TEXT("camera_state")));
                UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Camera State to: %d"), NewCameraState);
            }


            if(ActionName.StartsWith("move_camera"))
            {
                if (NewCameraState == 1)
                {
                    FVector NewLocation = GetActorLocation()+FVector(50.0f, 0.0f, 0.0f);

                    // Check if the proposed location is within the set limits.
                    if (NewLocation.X < CameraPositionMin.X || NewLocation.X > CameraPositionMax.X ||
                        NewLocation.Y < CameraPositionMin.Y || NewLocation.Y > CameraPositionMax.Y)
                    {
                        SetActorLocation(GetActorLocation()+FVector(-200.0f, 0.0f, 0.0f));
                    }else
                    {
                        SetActorLocation(NewLocation);
                    }
                }
                else if (NewCameraState == 2)
                {
                    FVector NewLocation = GetActorLocation()+FVector(-50.0f, 0.0f, 0.0f);

                    // Check if the proposed location is within the set limits.
                    if (NewLocation.X < CameraPositionMin.X || NewLocation.X > CameraPositionMax.X ||
                        NewLocation.Y < CameraPositionMin.Y || NewLocation.Y > CameraPositionMax.Y)
                    {
                        SetActorLocation(GetActorLocation()+FVector(200.0f, 0.0f, 0.0f));
                    }else
                    {
                        SetActorLocation(NewLocation);
                    }
                }
                else if (NewCameraState == 3)
                {
                    FVector NewLocation = GetActorLocation()+FVector(0.0f, 50.0f, 0.0f);

                    // Check if the proposed location is within the set limits.
                    if (NewLocation.X < CameraPositionMin.X || NewLocation.X > CameraPositionMax.X ||
                        NewLocation.Y < CameraPositionMin.Y || NewLocation.Y > CameraPositionMax.Y)
                    {
                        SetActorLocation(GetActorLocation()+FVector(0.0f, -200.0f, 0.0f));
                    }else
                    {
                        SetActorLocation(NewLocation);
                    }
                }
                else if (NewCameraState == 4)
                {
                    FVector NewLocation = GetActorLocation()+FVector(0.0f, -50.0f, 0.0f);

                    // Check if the proposed location is within the set limits.
                    if (NewLocation.X < CameraPositionMin.X || NewLocation.X > CameraPositionMax.X ||
                        NewLocation.Y < CameraPositionMin.Y || NewLocation.Y > CameraPositionMax.Y)
                    {
                        SetActorLocation(GetActorLocation()+FVector(0.0f, 200.0f, 0.0f));
                    }else
                    {
                        SetActorLocation(NewLocation);
                    }
                }
            }
            else if (ActionName == "switch_camera_state" || ActionName.StartsWith("switch_camera_state_ability") || ActionName.StartsWith("stop_move_camera") || ActionName == "change_ability_index")
            {
                SwitchControllerStateMachine(InputActionValue, NewCameraState);

                if (ActionName.StartsWith("switch_camera_state_ability"))
                {
                    ExtendedController->SetWorkArea(GetActorLocation());
                    ExtendedController->DropWorkArea();
                }
            }
            else if (ActionName == "left_click")
            {
                FVector StartLocation = GetActorLocation() + FVector(0, 0, 500.0f); // Start slightly above
                FVector EndLocationDown = GetActorLocation() - FVector(0, 0, 1000.0f); // Trace downwards
                FHitResult HitResult;
                FCollisionQueryParams CollisionParams;
                CollisionParams.AddIgnoredActor(this); // Ignore this actor
                bool IsHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocationDown, ECC_Visibility, CollisionParams);
                
                if (NewCameraState == 1 && IsHit)
                {
                    PerformLeftClickAction(HitResult, false);
                }else if (NewCameraState == 2 && IsHit)
                {
                    PerformLeftClickAction(HitResult, true);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Left Click: No ground hit found."));
                }
        
                //if (NewCameraState == 2)
                    //ExtendedController->LeftClickReleased();
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
            }  else if (ActionName == "resource_management")
            {
                if (NewCameraState == 1)
                    AddWorkerToResource(EResourceType::Primary, ExtendedController->SelectableTeamId);
                if (NewCameraState == 2)
                    RemoveWorkerFromResource(EResourceType::Primary, ExtendedController->SelectableTeamId);
                if (NewCameraState == 3)
                    AddWorkerToResource(EResourceType::Secondary, ExtendedController->SelectableTeamId);
                if (NewCameraState == 4)
                    RemoveWorkerFromResource(EResourceType::Secondary, ExtendedController->SelectableTeamId);
                if (NewCameraState == 5)
                    AddWorkerToResource(EResourceType::Tertiary, ExtendedController->SelectableTeamId);
                if (NewCameraState == 6)
                    RemoveWorkerFromResource(EResourceType::Tertiary, ExtendedController->SelectableTeamId);
            }
        } else
        {
            UE_LOG(LogTemp, Warning, TEXT("JSON Error: %s"), *JsonReader->GetErrorMessage());
        }
    }

    
    // Handle the initial setting of InputActionValue, Alt, and Ctrl if needed
    // (They are set before potentially calling other functions)
}

void ARLAgent::PerformRightClickAction(const FHitResult& HitResult)
{
 
    ACustomControllerBase* ExtendedController = Cast<ACustomControllerBase>(GetController());
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
                ExtendedController->RunUnitsAndSetWaypointsMass(HitResult);
                // RunUnitsAndSetWaypoints(HitResult, ExtendedController);
            }
        }
    }
 
    if (ExtendedController->SelectedUnits.Num() && ExtendedController->SelectedUnits[0] && ExtendedController->SelectedUnits[0]->CurrentDraggedWorkArea)
    {
        ExtendedController->DestroyDraggedArea(ExtendedController->SelectedUnits[0]);
    }
    
}

void ARLAgent::RunUnitsAndSetWaypoints(FHitResult Hit, AExtendedControllerBase* ExtendedController)
{
	int32 NumUnits = ExtendedController->SelectedUnits.Num();
	//int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)NumUnits));
	const int32 GridSize = ExtendedController->ComputeGridSize(NumUnits);
	AWaypoint* BWaypoint = nullptr;

	bool PlayWaypointSound = false;
	bool PlayRunSound = false;
	
	for (int32 i = 0; i < ExtendedController->SelectedUnits.Num(); i++) {
		if (ExtendedController->SelectedUnits[i] != ExtendedController->CameraUnitWithTag)
		if (ExtendedController->SelectedUnits[i] && ExtendedController->SelectedUnits[i]->UnitState != UnitData::Dead
		    && !ExtendedController->SelectedUnits[i]->IsWorker) {
			
			//FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
			int32 Row = i / GridSize;     // Row index
			int32 Col = i % GridSize;     // Column index

			//FVector RunLocation = Hit.Location + FVector(Col * 100, Row * 100, 0.f);  // Adjust x and y positions equally for a square grid
			FVector RunLocation = Hit.Location + ExtendedController->CalculateGridOffset(Row, Col);
		    bool HitNavModifier;
            RunLocation = ExtendedController->TraceRunLocation(RunLocation, HitNavModifier);
		    if (HitNavModifier) continue;
		    
			if(ExtendedController->SetBuildingWaypoint(RunLocation, ExtendedController->SelectedUnits[i], BWaypoint, PlayWaypointSound))
			{
				//PlayWaypointSound = true;
			}else if (ExtendedController->IsShiftPressed) {
				//DrawDebugSphere(GetWorld(), RunLocation, 15, 5, FColor::Green, false, 1.5, 0, 1);
				ExtendedController->DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				ExtendedController->RightClickRunShift(ExtendedController->SelectedUnits[i], RunLocation); // _Implementation
				PlayRunSound = true;
			}else if(ExtendedController->UseUnrealEnginePathFinding)
			{
				ExtendedController->DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				ExtendedController->RightClickRunUEPF(ExtendedController->SelectedUnits[i], RunLocation, true); // _Implementation
				PlayRunSound = true;
			}
			else {
				ExtendedController->DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				ExtendedController->RightClickRunDijkstraPF(ExtendedController->SelectedUnits[i], RunLocation, i); // _Implementation
				PlayRunSound = true;
			}
		}
	}

	if (ExtendedController->WaypointSound && PlayWaypointSound)
	{
		UGameplayStatics::PlaySound2D(this, ExtendedController->WaypointSound);
	}

	if (ExtendedController->RunSound && PlayRunSound)
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - ExtendedController->LastRunSoundTime >= ExtendedController->RunSoundDelayTime) // Check if 3 seconds have passed
		{
			UGameplayStatics::PlaySound2D(this, ExtendedController->RunSound);
			ExtendedController->LastRunSoundTime = CurrentTime; // Update the timestamp
		}
	}
}

void ARLAgent::PerformLeftClickAction(const FHitResult& HitResult, bool AttackToggled)
{
    
    ACustomControllerBase* CustomControllerBase = Cast<ACustomControllerBase>(GetController());
    if (!CustomControllerBase)
    {
        UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase in PerformLeftClickAction."));
        return;
    }
    
    CustomControllerBase->AbilityArrayIndex = 0;

    if (!CustomControllerBase->CameraBase || CustomControllerBase->CameraBase->TabToggled) return;

    if (CustomControllerBase->AltIsPressed)
    {
        CustomControllerBase->DestroyWorkArea();
        for (int32 i = 0; i < CustomControllerBase->SelectedUnits.Num(); i++)
        {
            CustomControllerBase->CancelAbilitiesIfNoBuilding(CustomControllerBase->SelectedUnits[i]);
        }
    }
    else if (AttackToggled)
    {
        int32 NumUnits = CustomControllerBase->SelectedUnits.Num();
        const int32 GridSize = CustomControllerBase->ComputeGridSize(NumUnits);
        AWaypoint* BWaypoint = nullptr;

        bool PlayWaypointSound = false;
        bool PlayAttackSound = false;

        for (int32 i = 0; i < CustomControllerBase->SelectedUnits.Num(); i++)
        {
            if (CustomControllerBase->SelectedUnits[i] != CustomControllerBase->CameraUnitWithTag && !CustomControllerBase->SelectedUnits[i]->IsWorker)
            {
                int32 Row = i / GridSize;     // Row index
                int32 Col = i % GridSize;     // Column index

                FVector RunLocation = HitResult.Location + CustomControllerBase->CalculateGridOffset(Row, Col);
                bool HitNavModifier;
                RunLocation = CustomControllerBase->TraceRunLocation(RunLocation, HitNavModifier);
                if (HitNavModifier) continue;
                
                if (CustomControllerBase->SetBuildingWaypoint(RunLocation, CustomControllerBase->SelectedUnits[i], BWaypoint, PlayWaypointSound))
                {
                    // Do Nothing
                }
                else
                {
                    CustomControllerBase->DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
                    CustomControllerBase->LeftClickAttackMass(CustomControllerBase->SelectedUnits[i], RunLocation, AttackToggled);
                    PlayAttackSound = true;
                }
            }

            if (CustomControllerBase->SelectedUnits[i])
                CustomControllerBase->FireAbilityMouseHit(CustomControllerBase->SelectedUnits[i], HitResult);
        }

        if (CustomControllerBase->WaypointSound && PlayWaypointSound)
        {
            UGameplayStatics::PlaySound2D(this, CustomControllerBase->WaypointSound);
        }

        if (CustomControllerBase->AttackSound && PlayAttackSound)
        {
            UGameplayStatics::PlaySound2D(this, CustomControllerBase->AttackSound);
        }
    } else {
        for (int32 i = 0; i < CustomControllerBase->SelectedUnits.Num(); i++)
        {
            if (CustomControllerBase->SelectedUnits[i] && !CustomControllerBase->SelectedUnits[i]->IsWorker && CustomControllerBase->SelectedUnits[i]->CurrentSnapshot.AbilityClass && CustomControllerBase->SelectedUnits[i]->CurrentDraggedAbilityIndicator)
            {
                CustomControllerBase->FireAbilityMouseHit(CustomControllerBase->SelectedUnits[i], HitResult);

            }
        }
    }

    CustomControllerBase->LeftClickIsPressed = false; // Reset the pressed state
}
void ARLAgent::AddWorkerToResource(EResourceType ResourceType, int TeamId)
{
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GameMode)
    {
        GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, 1); // Assuming this function exists in GameMode
    }
    //UpdateWidget();
}

void ARLAgent::RemoveWorkerFromResource(EResourceType ResourceType, int TeamId)
{
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GameMode)
    {
        GameMode->AddMaxWorkersForResourceType(TeamId, ResourceType, -1); // Assuming this function exists in GameMode
    }
    //UpdateWidget();
}

void ARLAgent::Server_PlayGame_Implementation(int32 SelectableTeamId)
{
    // Log entry point of the function
    UE_LOG(LogTemp, Log, TEXT("ARLAgent::Server_PlayGame_Implementation --- Called for Team ID: %d ---"), SelectableTeamId);

    // 1. Get the current game state (you need to implement this part)
    FGameStateData GameState = GatherGameState(SelectableTeamId);
    UE_LOG(LogTemp, Log, TEXT("Step 1/3: Game state gathered."));

    // 2. Get the action JSON from the inference component
    FString ActionJSON = InferenceComponent->ChooseJsonAction(GameState);

    // Log the action chosen by the model. This is the most important log.
    UE_LOG(LogTemp, Log, TEXT("Step 2/3: Inference component returned ActionJSON: %s"), *ActionJSON);

    // 3. Process the action using your existing function
    UE_LOG(LogTemp, Log, TEXT("Step 3/3: Passing ActionJSON to ReceiveRLAction for processing."));
    ReceiveRLAction(ActionJSON);
}

void ARLAgent::Server_RequestGameState_Implementation(int32 SelectableTeamId)
{
    // Gather game state data on the server.
    FGameStateData GameState = GatherGameState(SelectableTeamId);
    
    // Send the data back to the client.
    Client_ReceiveGameState(GameState);
}

void ARLAgent::Client_ReceiveGameState_Implementation(const FGameStateData& GameState)
{
    // Convert GameStateData to JSON (or another format your RL process understands)
    FString GameStateJSON = CreateGameStateJSON(GameState);
    
    // Write to shared memory
    if (SharedMemoryManager)
    {
        SharedMemoryManager->WriteGameState(GameStateJSON);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SharedMemoryManager is not valid, cannot write game state."));
    }
    
    // Check for new actions
    CheckForNewActions();
}

FGameStateData ARLAgent::GatherGameState(int32 SelectableTeamId)
{
    FGameStateData GameState;
    
    AGameModeBase* BaseGameMode = GetWorld()->GetAuthGameMode();
    AUpgradeGameMode* GameMode = Cast<AUpgradeGameMode>(BaseGameMode);
    if (!GameMode) return GameState;

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
                if (MyUnit->TeamId == SelectableTeamId)
                {
                    GameState.MyUnitCount++;
                    GameState.MyTotalHealth += Attributes->GetHealth();
                    GameState.MyTotalAttackDamage += Attributes->GetAttackDamage();
                    SumFriendlyPositions += MyUnit->GetActorLocation();
                    NumFriendlyUnits++;
                }
                else
                {
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
    if (GameMode)
    {
        int32 MyTeamId = SelectableTeamId;
        GameState.PrimaryResource = GameMode->GetResource(MyTeamId, EResourceType::Primary);
        GameState.SecondaryResource = GameMode->GetResource(MyTeamId, EResourceType::Secondary);
        GameState.TertiaryResource = GameMode->GetResource(MyTeamId, EResourceType::Tertiary);
        GameState.RareResource = GameMode->GetResource(MyTeamId, EResourceType::Rare);
        GameState.EpicResource = GameMode->GetResource(MyTeamId, EResourceType::Epic);
        GameState.LegendaryResource = GameMode->GetResource(MyTeamId, EResourceType::Legendary);
    }

    return GameState;
}