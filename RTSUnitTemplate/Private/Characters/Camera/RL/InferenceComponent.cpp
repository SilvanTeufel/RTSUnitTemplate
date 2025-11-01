// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Camera/RL/InferenceComponent.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h" // Important: Include the specific CPU runtime header
#include "NNETypes.h"
#include "NNERuntimeRunSync.h"     // <-- ADD THIS for FTensorBinding
#include "NNEClasses.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"

// Bring the NNE namespace into scope to simplify type names
using namespace UE::NNE;

UInferenceComponent::UInferenceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    QNetworkModelData = nullptr;
    InitializeActionSpace();
}

UInferenceComponent::~UInferenceComponent()
{
    // Smart pointers (TSharedPtr) will automatically clean up the model and instance
}

void UInferenceComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!QNetworkModelData)
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: QNetworkModelData asset is not assigned!"));
        return;
    }

    // Get the specific CPU runtime for ONNX
    TWeakInterfacePtr<INNERuntimeCPU> Runtime = GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));
    if (!Runtime.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: NNERuntimeORTCpu could not be found!"));
        return;
    }

    // Create a model compatible with the CPU
    RuntimeModel = Runtime->CreateModelCPU(QNetworkModelData);
    if (!RuntimeModel.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Failed to create a CPU Model from the provided asset."));
        return;
    }

    // From the model, create an instance that can be run
    ModelInstance = RuntimeModel->CreateModelInstanceCPU();
    if (!ModelInstance.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Failed to create a CPU Model Instance."));
        return;
    }

    // Define the shape of the input tensor: [batch_size, num_features]
    const FTensorShape InputShape = FTensorShape::Make({1, 21});
    TArray<FTensorShape> InputShapes;
    InputShapes.Add(InputShape);

    // Set the input shapes for the model instance. This is a required one-time setup.
    if (ModelInstance->SetInputTensorShapes(InputShapes) != EResultStatus::Ok)
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Failed to set the input tensor shapes."));
        ModelInstance.Reset(); // Invalidate the instance if setup fails
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("InferenceComponent: Runtime model and instance created successfully."));
}

TArray<float> UInferenceComponent::ConvertStateToArray(const FGameStateData& GameStateData) const
{
    TArray<float> StateArray;
    // Pre-size the array for performance. The state size is 21 floats.
    StateArray.Reserve(21); 

    // The order here MUST EXACTLY match the order used to train the model.
    StateArray.Add(static_cast<float>(GameStateData.MyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.EnemyUnitCount));
    StateArray.Add(GameStateData.MyTotalHealth);
    StateArray.Add(GameStateData.EnemyTotalHealth);
    StateArray.Add(GameStateData.MyTotalAttackDamage);
    StateArray.Add(GameStateData.EnemyTotalAttackDamage);
    
    // Add FVector components
    StateArray.Add(GameStateData.AgentPosition.X);
    StateArray.Add(GameStateData.AgentPosition.Y);
    StateArray.Add(GameStateData.AgentPosition.Z);
    StateArray.Add(GameStateData.AverageFriendlyPosition.X);
    StateArray.Add(GameStateData.AverageFriendlyPosition.Y);
    StateArray.Add(GameStateData.AverageFriendlyPosition.Z);
    StateArray.Add(GameStateData.AverageEnemyPosition.X);
    StateArray.Add(GameStateData.AverageEnemyPosition.Y);
    StateArray.Add(GameStateData.AverageEnemyPosition.Z);

    // Add resource counts
    StateArray.Add(GameStateData.PrimaryResource);
    StateArray.Add(GameStateData.SecondaryResource);
    StateArray.Add(GameStateData.TertiaryResource);
    StateArray.Add(GameStateData.RareResource);
    StateArray.Add(GameStateData.EpicResource);
    StateArray.Add(GameStateData.LegendaryResource);

    return StateArray;
}

FString UInferenceComponent::GetActionAsJSON(int32 ActionIndex)
{
    if (!ActionSpace.IsValidIndex(ActionIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Invalid ActionIndex %d provided."), ActionIndex);
        return TEXT("{}"); // Return an empty JSON object on error
    }

    const FRLAction& SelectedAction = ActionSpace[ActionIndex];

    // Create a JSON object and populate it with data from the selected action
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("type"), SelectedAction.Type);
    JsonObject->SetNumberField(TEXT("input_value"), SelectedAction.InputValue);
    JsonObject->SetBoolField(TEXT("alt"), SelectedAction.bAlt);
    JsonObject->SetBoolField(TEXT("ctrl"), SelectedAction.bCtrl);
    JsonObject->SetStringField(TEXT("action"), SelectedAction.Action);
    JsonObject->SetNumberField(TEXT("camera_state"), SelectedAction.CameraState);

    // Serialize the JSON object to a string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    return OutputString;
}

void UInferenceComponent::InitializeActionSpace()
{
  ActionSpace = {
        // Ctrl + Q, W, E, R
        {"Control", 1.0f, false, true, "switch_camera_state", 18}, // R
        {"Control", 1.0f, false, true, "switch_camera_state", 9},  // Q
        {"Control", 1.0f, false, true, "switch_camera_state", 10}, // E
        {"Control", 1.0f, false, true, "switch_camera_state", 1},  // W

        // Ctrl + 1-6
        {"Control", 1.0f, false, true, "switch_camera_state", 21},
        {"Control", 1.0f, false, true, "switch_camera_state", 22},
        {"Control", 1.0f, false, true, "switch_camera_state", 23},
        {"Control", 1.0f, false, true, "switch_camera_state", 24},
        {"Control", 1.0f, false, true, "switch_camera_state", 25},
        {"Control", 1.0f, false, true, "switch_camera_state", 26},

        // No Modifier + Use Ability 1-6
        {"Control", 1.0f, false, false, "switch_camera_state_ability", 21},
        {"Control", 1.0f, false, false, "switch_camera_state_ability", 22},
        {"Control", 1.0f, false, false, "switch_camera_state_ability", 23},
        {"Control", 1.0f, false, false, "switch_camera_state_ability", 24},
        {"Control", 1.0f, false, false, "switch_camera_state_ability", 25},
        {"Control", 1.0f, false, false, "switch_camera_state_ability", 26},

        // Change Ability Index
        {"Control", 1.0f, false, false, "change_ability_index", 13},

        // No Modifier + Move Camera
        {"Control", 1.0f, false, false, "move_camera", 1},
        {"Control", 1.0f, false, false, "move_camera", 2},
        {"Control", 1.0f, false, false, "move_camera", 3},
        {"Control", 1.0f, false, false, "move_camera", 4},

        // Set workers to Resource
        {"Control", 1.0f, false, false, "resource_management", 1},
        {"Control", 1.0f, false, false, "resource_management", 2},
        {"Control", 1.0f, false, false, "resource_management", 3},
        {"Control", 1.0f, false, false, "resource_management", 4},
        {"Control", 1.0f, false, false, "resource_management", 5},
        {"Control", 1.0f, false, false, "resource_management", 6},

        // No Modifier + Left Click (Move and Attack)
        {"Control", 1.0f, false, false, "left_click", 1},
        {"Control", 1.0f, false, false, "left_click", 2},

        // No Modifier + Right Click
        {"Control", 1.0f, false, false, "right_click", 1}
    };
}

int32 UInferenceComponent::ChooseAction(const TArray<float>& GameState)
{
    // --- Basic Checks ---
    if (!ModelInstance.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent: ChooseAction called but the model instance is not valid."));
        return 0;
    }

    if (GameState.Num() != 21)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent: Invalid GameState size. Expected 21, got %d."), GameState.Num());
        return 0;
    }

    // --- Prepare Input Tensor ---
    // The input tensor shape is [1, 21] (batch_size=1, num_features=21)
    FTensorShape InputShape = FTensorShape::Make({1, 21});
    
    TArray<FTensorBindingCPU> InputBindings;
    InputBindings.Emplace();
    InputBindings[0].Data = const_cast<float*>(GameState.GetData());
    InputBindings[0].SizeInBytes = GameState.Num() * sizeof(float); // Provide pointer and size in bytes

    // --- Prepare Output Tensor ---
    TArray<float> QValues;
    QValues.SetNumZeroed(ActionSpace.Num()); // The size of your ACTION_SPACE

    TArray<FTensorBindingCPU> OutputBindings;
    OutputBindings.Emplace();
    OutputBindings[0].Data = QValues.GetData();
    OutputBindings[0].SizeInBytes = QValues.Num() * sizeof(float); // Provide pointer and size in bytes

    
    // --- Run Inference ---
    if (ModelInstance->RunSync(InputBindings, OutputBindings) != EResultStatus::Ok)
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Model execution failed!"));
        return 0;
    }
    
    // --- Process Output (Argmax) ---
    // The QValues array has now been filled by the model
    int32 BestActionIndex = 0;
    float MaxQValue = -FLT_MAX; // Use the smallest possible float for correct comparison

    if (QValues.Num() > 0)
    {
        MaxQValue = QValues[0];
        for (int32 i = 1; i < QValues.Num(); ++i)
        {
            if (QValues[i] > MaxQValue)
            {
                MaxQValue = QValues[i];
                BestActionIndex = i;
            }
        }
    }
   
    
    return BestActionIndex;
}

FString UInferenceComponent::GetActionFromRLModel(const FGameStateData& GameState)
{
    // Convert the input struct to the flat TArray<float> the model needs
    const TArray<float> GameStateArray = ConvertStateToArray(GameState);

    // --- Basic Checks ---
    if (!ModelInstance.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent: ChooseAction called but the model instance is not valid."));
        return TEXT("{}");
    }

    // This check is still useful to ensure the conversion function is correct
    if (GameStateArray.Num() != 21)
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Converted GameState size is incorrect. Expected 21, got %d."), GameStateArray.Num());
        return TEXT("{}");
    }

    // --- Prepare Input & Output Tensors ---
    TArray<UE::NNE::FTensorBindingCPU> InputBindings;
    InputBindings.Emplace();
    // Use the converted GameStateArray here
    InputBindings[0].Data = const_cast<float*>(GameStateArray.GetData()); 
    InputBindings[0].SizeInBytes = GameStateArray.Num() * sizeof(float);

    TArray<float> QValues;
    QValues.SetNumZeroed(ActionSpace.Num());

    TArray<UE::NNE::FTensorBindingCPU> OutputBindings;
    OutputBindings.Emplace();
    OutputBindings[0].Data = QValues.GetData();
    OutputBindings[0].SizeInBytes = QValues.Num() * sizeof(float);

    // --- Run Inference ---
    if (ModelInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Model execution failed!"));
        return TEXT("{}");
    }
    
    // --- Process Output (Argmax) ---
    int32 BestActionIndex = 0;
    if (QValues.Num() > 0)
    {
        float MaxQValue = QValues[0];
        for (int32 i = 1; i < QValues.Num(); ++i)
        {
            if (QValues[i] > MaxQValue)
            {
                MaxQValue = QValues[i];
                BestActionIndex = i;
            }
        }
    }
    
    // --- Return the chosen action as a JSON string ---
    return GetActionAsJSON(BestActionIndex);
}

FString UInferenceComponent::ChooseJsonAction(const FGameStateData& GameState)
{
    if (BrainMode == EBrainMode::RL_Model)
    {
        // --- 1. Use the RL Brain ---
        return GetActionFromRLModel(GameState);
    }
    
    if (BrainMode == EBrainMode::Behavior_Tree && BlackboardComp && BehaviorTreeComp)
    {
        // --- 2. Use the BT Brain ---
        
        // A. Update the Blackboard with the latest game state
        UpdateBlackboard(GameState);

        // B. Clear the old action from the Blackboard
        BlackboardComp->SetValueAsString(TEXT("SelectedActionJSON"), TEXT(""));

        // C. Tick the Behavior Tree (using 0.1f from ARLAgent's timer)
        BehaviorTreeComp->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

        // D. Get the action chosen by the BT task
        FString ChosenAction = BlackboardComp->GetValueAsString(TEXT("SelectedActionJSON"));

        if (!ChosenAction.IsEmpty())
        {
            return ChosenAction;
        }

        // Failsafe: if BT doesn't pick an action, do nothing
        return TEXT("{}"); 
    }

    UE_LOG(LogTemp, Warning, TEXT("ChooseJsonAction: No valid brain mode selected or BT not initialized."));
    return TEXT("{}");
}

void UInferenceComponent::InitializeBehaviorTree(AController* OwnerController)
{
    if (BrainMode != EBrainMode::Behavior_Tree || !StrategyBehaviorTree || !OwnerController)
    {
        // Only init if in BT mode and all assets are correctly set
        return; 
    }

    // BT components are owned by the Controller
    BlackboardComp = NewObject<UBlackboardComponent>(OwnerController, TEXT("BlackboardComp"));
    if (BlackboardComp)
    {
        BlackboardComp->InitializeBlackboard(*StrategyBehaviorTree->BlackboardAsset);
    }

    BehaviorTreeComp = NewObject<UBehaviorTreeComponent>(OwnerController, TEXT("BehaviorTreeComp"));
    if (BehaviorTreeComp && BlackboardComp)
    {
        BehaviorTreeComp->StartTree(*StrategyBehaviorTree);
        UE_LOG(LogTemp, Log, TEXT("InferenceComponent: Behavior Tree initialized and started."));
    }
}

void UInferenceComponent::UpdateBlackboard(const FGameStateData& GameState)
{
    if (!BlackboardComp) return;

    // Map all your GameState data to Blackboard keys.
    // The BT will use these keys to make decisions.
    BlackboardComp->SetValueAsInt(TEXT("MyUnitCount"), GameState.MyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("EnemyUnitCount"), GameState.EnemyUnitCount);
    BlackboardComp->SetValueAsFloat(TEXT("MyTotalHealth"), GameState.MyTotalHealth);
    BlackboardComp->SetValueAsFloat(TEXT("EnemyTotalHealth"), GameState.EnemyTotalHealth);
    
    BlackboardComp->SetValueAsFloat(TEXT("PrimaryResource"), GameState.PrimaryResource);
    BlackboardComp->SetValueAsFloat(TEXT("SecondaryResource"), GameState.SecondaryResource);

    BlackboardComp->SetValueAsVector(TEXT("AgentPosition"), GameState.AgentPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageFriendlyPosition"), GameState.AverageFriendlyPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageEnemyPosition"), GameState.AverageEnemyPosition);

    // NOTE: You should add GameTime to your FGameStateData struct and update it here
    // BlackboardComp->SetValueAsFloat(TEXT("GameTime"), GameState.GameTime);
}
