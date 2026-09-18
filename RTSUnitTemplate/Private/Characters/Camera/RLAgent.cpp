// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameModes/UpgradeGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Actors/WorkArea.h"   // Bauwarteschlange im Zustandsvektor
#include "Components/SceneComponent.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"

// Gruppenwahl (Aktionen 0-9) und Faehigkeitsdruck (10-15) sind im Aktionsraum getrennt. Zwischen
// beiden liegen im Schnitt mehrere andere Zuege - 27 % davon Kamerafahrten - und die Auswahl
// ueberlebt das nicht. Die Regel-KI hat das Problem nicht, weil BuildCompositeActionJSON Wahl und
// Druck in EINE Entscheidung packt; Verhaltensklonen kann diese Kopplung deshalb gar nicht lernen.
// 1 = der Druck stellt bei leerer Auswahl die zuletzt erfolgreiche Gruppe wieder her.
static int32 GRLAbilityReselect = 0;
static FAutoConsoleVariableRef CVarRLAbilityReselect(
    TEXT("rts.rl.ability.reselect"),
    GRLAbilityReselect,
    TEXT("1 = ein Faehigkeitsdruck des Netzes stellt bei leerer Auswahl die zuletzt gewaehlte Gruppe wieder her (nur KI-Pfad)."),
    ECVF_Default);

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
    CameraBoundsReference = GetActorLocation();
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("[RLAgent] BeginPlay on %s Controller=%s HasAuthority=%s"), *GetNameSafe(this), *GetNameSafe(GetController()), HasAuthority() ? TEXT("true") : TEXT("false"));
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

            // Create SharedMemoryManager only if explicitly enabled
            if (bEnableSharedMemoryIO)
            {
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Creating SharedMemoryManager (bEnableSharedMemoryIO=true)."));
                SharedMemoryManager = new FSharedMemoryManager(*MemoryName, sizeof(SharedData));
            }
            else
            {
                SharedMemoryManager = nullptr;
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] SharedMemory disabled (bEnableSharedMemoryIO=false), skipping creation."));
            }
        }

        // --- Initialize Behavior Tree brain components if enabled ---
        if (InferenceComponent)
        {
            AController* C = GetController();
            if (AAIController* AI = Cast<AAIController>(C))
            {
                InferenceComponent->InitializeBehaviorTree(AI);
            }
            else
            {
                if (bDebug) UE_LOG(LogTemp, Warning, TEXT("ARLAgent: Skipping InferenceComponent::InitializeBehaviorTree because controller is not an AAIController. Expect ARTSBTController::OnPossess to start the BT."));
            }
        }
    }
    
    SIZE_T MemorySizeNeeded = sizeof(SharedData);
    
    if (GetWorld() && GetWorld()->IsNetMode(ENetMode::NM_Client))
    {
        if (SharedMemoryManager)
        {
            if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] SharedMemoryManager created successfully with name: %s and size: %lld."), *MemoryName, MemorySizeNeeded);
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Failed to create SharedMemoryManager."));
        }
    }

    FInputActionValue InputActionValue;
    InputActionValue = FInputActionValue(1.0f);
    Input_Tab_Released(InputActionValue, 0);
    // Set up a timer to update the game state only when SharedMemory IO is enabled (RL mode)
    if (bEnableSharedMemoryIO)
    {
        GetWorldTimerManager().SetTimer(RLUpdateTimerHandle, this, &ARLAgent::UpdateGameState, 0.1f, true);
    }

    // In BT mode (or whenever SharedMemory IO is disabled), expect a BT Service to feed the Blackboard.
    if (!bEnableSharedMemoryIO)
    {
        if (bDebug) UE_LOG(LogTemp, Log, TEXT("ARLAgent: Expecting BT Service (UBTService_PushGameStateToBB) to feed Blackboard (no agent-side or controller-side BB timer)."));
    }
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

    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("ARLAgent::ReceiveRLAction: %s"), *ActionJSON);
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
                if (bDebug) UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Received invalid Action JSON."));
                return;
            }

            FInputActionValue InputActionValue(1.0f);
            int32 NewCameraState = 0;
            FString ActionName = Action->GetStringField(TEXT("action"));

            AExtendedControllerBase* ExtendedController = Cast<AExtendedControllerBase>(GetController());
            if (!ExtendedController)
            {
                if (bDebug) UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase."));
                return;
            }
            

            if (Action->HasField(TEXT("input_value")))
            {
                InputActionValue = FInputActionValue(static_cast<float>(Action->GetNumberField(TEXT("input_value"))));
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Input Value to: %.2f"), InputActionValue.Get<float>());
            }
            if (Action->HasField(TEXT("alt")))
            {
                ExtendedController->AltIsPressed = Action->GetBoolField(TEXT("alt"));
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Alt to: %s"), ExtendedController->AltIsPressed ? TEXT("True") : TEXT("False"));
            }
            if (Action->HasField(TEXT("ctrl")))
            {
                ExtendedController->IsCtrlPressed = Action->GetBoolField(TEXT("ctrl"));
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Setting Ctrl to: %s"), ExtendedController->IsCtrlPressed ? TEXT("True") : TEXT("False"));
            }
            // ExtendedController->SetModifierKeys(AltIsPressed, CtrlIsPressed);

            if (Action->HasField(TEXT("camera_state")))
            {
                NewCameraState = static_cast<int32>(Action->GetNumberField(TEXT("camera_state")));
                // if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] NewCameraState: %d"), NewCameraState);
            }

            // Intent flag for the click that follows. Reset every action so it never leaks into the
            // next decision - a stale "true" would hijack ordinary move orders.
            bActionAimsAtTransporter = Action->HasField(TEXT("aim_at_transporter"))
                                       && Action->GetBoolField(TEXT("aim_at_transporter"));

            // Which ability array this action means. It has to be applied HERE, immediately before the
            // press: setting it on the controller when the rule was chosen let the next decision
            // overwrite it first, so an "OrbitalUplink" rule ended up pressing array 0 and built a
            // BioIntegrator instead (measured 16 of them instead of the 2 the cap allows).
            if (Action->HasField(TEXT("ability_array_index")))
            {
                if (ACameraControllerBase* CamCtrl = Cast<ACameraControllerBase>(ExtendedController))
                {
                    CamCtrl->AbilityArrayIndex = FMath::Clamp(
                        static_cast<int32>(Action->GetNumberField(TEXT("ability_array_index"))), 0, 3);
                }
            }

            /*
            if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Pre-Action State: ActionName=%s, SelectedUnits=%d, IsCtrl=%s, Alt=%s"), 
                *ActionName, ExtendedController->SelectedUnits.Num(), 
                ExtendedController->IsCtrlPressed ? TEXT("True") : TEXT("False"), 
                ExtendedController->AltIsPressed ? TEXT("True") : TEXT("False"));
            */

            if(ActionName.StartsWith("move_camera"))
            {
                // Helper that moves the camera while constraining it to an optional bounding box actor
                // placed in the level and tagged with "RLAgentCameraBounds". If no such actor exists,
                // it falls back to the legacy min/max checks and bounce behavior.
                auto MoveWithBounds = [&](const FVector& Delta, const FVector& FallbackBounceDelta)
                {
                    const FVector CurrentLocation = GetActorLocation();
                    const FVector Proposed = CurrentLocation + Delta;

                    // Try to find a bounds provider: 1) Actor tag; 2) Component tag on any component (e.g., BoxComponent)
                    TArray<AActor*> BoundsActors;
                    FBox BoundsBox(EForceInit::ForceInit);
                    bool bHasBounds = false;
                    if (GetWorld())
                    {
                        // Prefer an Actor with the tag; then try to use its BoxComponent (or any primitive component) tagged with the same tag
                        UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("RLAgentCameraBounds")), BoundsActors);

                        // With more than one AI in the level each side needs its own box, otherwise every
                        // agent is pinned to whichever one happened to be found first and builds its base
                        // across the map. Each agent takes the box it started inside, or else the closest.
                        AActor* BoundsActor = nullptr;
                        {
                            double BestDistSq = TNumericLimits<double>::Max();
                            for (AActor* Candidate : BoundsActors)
                            {
                                if (!Candidate) continue;

                                FVector CandOrigin, CandExtent;
                                Candidate->GetActorBounds(true, CandOrigin, CandExtent);
                                const FBox CandBox(CandOrigin - CandExtent, CandOrigin + CandExtent);

                                if (CandBox.IsInsideXY(CameraBoundsReference))
                                {
                                    BoundsActor = Candidate;
                                    break;
                                }

                                const double DistSq = FVector::DistSquared2D(CandOrigin, CameraBoundsReference);
                                if (DistSq < BestDistSq)
                                {
                                    BestDistSq = DistSq;
                                    BoundsActor = Candidate;
                                }
                            }
                        }

                        if (BoundsActor)
                        {
                            // Look for a component on this actor with ComponentTag "RLAgentCameraBounds"
                            TInlineComponentArray<UActorComponent*> Comps;
                            BoundsActor->GetComponents(Comps);
                            for (UActorComponent* Comp : Comps)
                            {
                                if (Comp && Comp->ComponentHasTag(FName(TEXT("RLAgentCameraBounds"))))
                                {
                                    if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
                                    {
                                        const FBoxSphereBounds B = Prim->Bounds;
                                        BoundsBox = FBox(B.Origin - B.BoxExtent, B.Origin + B.BoxExtent);
                                        bHasBounds = true;
                                        if (bDebug) UE_LOG(LogTemp, Log, TEXT("[RLAgent] Camera bounds from Component tag on: %s.%s"), *BoundsActor->GetName(), *Comp->GetName());
                                    }
                                    else
                                    {
                                        // Use owner's bounds as a fallback if the component is not primitive
                                        FVector Origin, Extent;
                                        BoundsActor->GetActorBounds(true, Origin, Extent);
                                        BoundsBox = FBox(Origin - Extent, Origin + Extent);
                                        bHasBounds = true;
                                        if (bDebug) UE_LOG(LogTemp, Log, TEXT("[RLAgent] Camera bounds from non-primitive Component tag on: %s.%s (using owner bounds)"), *BoundsActor->GetName(), *Comp->GetName());
                                    }
                                    break;
                                }
                            }

                            // If no tagged component found on the tagged actor, fallback to actor bounds
                            if (!bHasBounds)
                            {
                                FVector Origin, Extent;
                                BoundsActor->GetActorBounds(true, Origin, Extent);
                                BoundsBox = FBox(Origin - Extent, Origin + Extent);
                                bHasBounds = true;
                                if (bDebug) UE_LOG(LogTemp, Log, TEXT("[RLAgent] Camera bounds from Actor tag on: %s (no tagged component found; using actor bounds)"), *BoundsActor->GetName());
                            }
                        }
                    }

                    if (bHasBounds)
                    {
                        FVector Clamped = Proposed;
                        Clamped.X = FMath::Clamp(Clamped.X, BoundsBox.Min.X, BoundsBox.Max.X);
                        Clamped.Y = FMath::Clamp(Clamped.Y, BoundsBox.Min.Y, BoundsBox.Max.Y);
                        SetActorLocation(Clamped);
                    }
                    else
                    {
                        // Fallback to legacy behavior if no bounds actor is placed in the map.
                        if (Proposed.X < CameraPositionMin.X || Proposed.X > CameraPositionMax.X ||
                            Proposed.Y < CameraPositionMin.Y || Proposed.Y > CameraPositionMax.Y)
                        {
                            SetActorLocation(CurrentLocation + FallbackBounceDelta);
                        }
                        else
                        {
                            SetActorLocation(Proposed);
                        }
                    }
                };

                if (NewCameraState == 1)
                {
                    MoveWithBounds(FVector(DeltaMovement, 0.0f, 0.0f), FVector(-FallbackBounceDelta, 0.0f, 0.0f));
                }
                else if (NewCameraState == 2)
                {
                    MoveWithBounds(FVector(-DeltaMovement, 0.0f, 0.0f), FVector(FallbackBounceDelta, 0.0f, 0.0f));
                }
                else if (NewCameraState == 3)
                {
                    MoveWithBounds(FVector(0.0f, DeltaMovement, 0.0f), FVector(0.0f, -FallbackBounceDelta, 0.0f));
                }
                else if (NewCameraState == 4)
                {
                    MoveWithBounds(FVector(0.0f, -DeltaMovement, 0.0f), FVector(0.0f, FallbackBounceDelta, 0.0f));
                }
            }
            else if (ActionName == "switch_camera_state" || ActionName.StartsWith("switch_camera_state_ability") || ActionName.StartsWith("stop_move_camera") || ActionName == "change_ability_index")
            {
                // This used to abort the WHOLE action when SelectedUnits[0] happened to hold a BuildArea.
                // The agent selects every worker it owns, so index 0 is arbitrary - and self-reinforcing:
                // the worker that took the last build order is exactly the one carrying a BuildArea, so
                // from then on every further ability action was dropped until it finished. Whole factions
                // built nothing because of this. Only give up when NOBODY in the selection is free.
                const bool bIstFaehigkeitsdruck = ActionName.StartsWith("switch_camera_state_ability");

                // Nur eingreifen, wenn die Auswahl leer IST - ein Druck mit gueltiger Auswahl
                // bleibt unveraendert. Der Eingriff betrifft ausschliesslich den KI-Pfad.
                if (bIstFaehigkeitsdruck && GRLAbilityReselect != 0
                    && ExtendedController->SelectedUnits.Num() == 0 && LetzteAuswahlTaste >= 0)
                {
                    // Die Gruppenwahl-Aktionen tragen ctrl=true, der Faehigkeitsdruck ctrl=false -
                    // und ReceiveRLAction hat das oben bereits auf den Controller geschrieben. Ohne
                    // das Setzen hier druecke ich "1" statt "Ctrl+1", was gar keine Gruppenwahl ist:
                    // gemessen am 01.09.2026 waren 869 von 869 Nachwahlen erfolglos.
                    const bool bCtrlVorher = ExtendedController->IsCtrlPressed;
                    ExtendedController->IsCtrlPressed = true;
                    SwitchControllerStateMachine(InputActionValue, LetzteAuswahlTaste);
                    ExtendedController->IsCtrlPressed = bCtrlVorher;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[NetzDruck] Team %d Nachwahl Gruppe=%d -> Auswahl=%d"),
                        ExtendedController->SelectableTeamId, LetzteAuswahlTaste,
                        ExtendedController->SelectedUnits.Num());
                }

                {
                    bool bAnyFree = false;
                    for (AUnitBase* Selected : ExtendedController->SelectedUnits)
                    {
                        if (IsValid(Selected) && Selected->BuildArea == nullptr)
                        {
                            bAnyFree = true;
                            break;
                        }
                    }

                    // [NetzDruck] Gemessen am 01.09.2026: das Netz waehlt in 24,1 % seiner Zuege eine
                    // Faehigkeit (Regel-KI: 36,6 %), loest ueber eine ganze Messreihe aber nur 118
                    // Ablehnungen in ActivateAbilityByInputID aus - gegen 14 710 der Regel-KI. Seine
                    // Bauwuensche kommen dort also gar nicht erst an. Diese Zeile sagt, wie weit sie
                    // kommen; ohne sie ist eine leere Auswahl von einem gelungenen Bau nicht zu
                    // unterscheiden.
                    if (bIstFaehigkeitsdruck)
                    {
                        // Welche Faehigkeit eine Taste ausloest, haengt nicht nur an der Taste,
                        // sondern am Array-Index - und daran, WELCHE Einheiten ausgewaehlt sind:
                        // der Druck feuert auf alle. Gemessen am 01.09.2026 loeste das Netz
                        // ueberwiegend Ein-/Ausgraben aus (4 von 147 Aktivierungen waren ein Bau).
                        // Diese beiden Felder trennen "falscher Index" von "falsche Einheiten".
                        int32 Arbeiter = 0;
                        for (AUnitBase* Selected : ExtendedController->SelectedUnits)
                        {
                            if (IsValid(Selected) && Selected->IsWorker) { ++Arbeiter; }
                        }
                        UE_LOG(LogTemp, Warning,
                            TEXT("[NetzDruck] Team %d Taste=%d Auswahl=%d frei=%d Index=%d Arbeiter=%d"),
                            ExtendedController->SelectableTeamId, NewCameraState,
                            ExtendedController->SelectedUnits.Num(), bAnyFree ? 1 : 0,
                            ExtendedController->AbilityArrayIndex, Arbeiter);
                    }

                    if (!bAnyFree && ExtendedController->SelectedUnits.Num() > 0)
                    {
                        if (bIstFaehigkeitsdruck)
                        {
                            UE_LOG(LogTemp, Warning,
                                TEXT("[NetzDruck] Team %d VERWORFEN: alle %d ausgewaehlten Einheiten bauen schon"),
                                ExtendedController->SelectableTeamId, ExtendedController->SelectedUnits.Num());
                        }
                        if (bDebug) UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Every selected unit is already building."));
                        return;
                    }
                }
                
                bool bSkipSwitch = false;
                // If this is an ability action, check if the unit is already busy with another ability.
                if (ActionName.StartsWith("switch_camera_state_ability") && NewCameraState >= 21 && NewCameraState <= 26)
                {
                    if (ExtendedController->SelectedUnits.Num() > 0 && ExtendedController->SelectedUnits[0])
                    {
                            // If it's a worker, we don't skip, because it might be a multi-step ability activation (e.g. build -> place)
                            if (ExtendedController->SelectedUnits[0]->ActivatedAbilityInstance != nullptr && !ExtendedController->SelectedUnits[0]->IsWorker)
                            {
                                bSkipSwitch = true;
                            }
                    }
                }
                
                if (!bSkipSwitch)
                {
                    SwitchControllerStateMachine(InputActionValue, NewCameraState);
                }

                // [NetzDruck] Ohne diese Zeile sind zwei Ursachen nicht zu trennen: das Netz waehlt
                // Gruppen, die gar keine Einheiten haben - oder die Auswahl geht bis zum Druck
                // wieder verloren. Erst der Vergleich beider Zeilen entscheidet das.
                if (ActionName == "switch_camera_state")
                {
                    const int32 NachDerWahl = ExtendedController->SelectedUnits.Num();
                    UE_LOG(LogTemp, Warning, TEXT("[NetzDruck] Team %d Gruppenwahl=%d Auswahl=%d"),
                        ExtendedController->SelectableTeamId, NewCameraState, NachDerWahl);
                    if (NachDerWahl > 0)
                    {
                        LetzteAuswahlTaste = NewCameraState;
                    }
                }

                // if (bDebug) UE_LOG(LogTemp, Log, TEXT("[ARLAgent] Post-Switch State: SelectedUnits=%d"), ExtendedController->SelectedUnits.Num());
                
                if (ActionName.StartsWith("switch_camera_state_ability"))
                {
                    // If we have a worker selected, we perform the specialized "Drop Work Area" logic.
                    // If NOT a worker (e.g., a building), SwitchControllerStateMachine already triggered ExecuteOnAbilityInputDetected,
                    // so we skip the worker-specific drop logic to avoid conflicts.
                    bool bIsWorker = (ExtendedController->SelectedUnits.Num() > 0 && ExtendedController->SelectedUnits[0] && ExtendedController->SelectedUnits[0]->IsWorker);

                    if (bIsWorker)
                    {
                        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("TRYING DROPPING WORKAREA"));

                        if (ExtendedController->SelectedUnits.Num() > 0 && ExtendedController->SelectedUnits[0])
                        {
                            // The build ability spawned the work-area ghost during the SwitchControllerStateMachine
                            // call above, so this frame its bounds and overlap set are still empty. Validating now
                            // makes PerformWorkAreaDistanceResolution bail out and GetOverlappingActors come back
                            // empty, which is how the AI ended up stacking buildings on top of each other. One tick
                            // later the ghost is fully registered and the normal placement rules do their job.
                            // Drop the ghost for the worker that actually RECEIVED it. The controller no
                            // longer forces the order onto SelectedUnits[0] for the AI, so assuming index 0
                            // here would hand the drop to a worker holding nothing while the real ghost
                            // stayed stuck on another one.
                            AUnitBase* GhostOwner = ExtendedController->SelectedUnits[0];
                            for (AUnitBase* Selected : ExtendedController->SelectedUnits)
                            {
                                if (IsValid(Selected) && Selected->CurrentDraggedWorkArea)
                                {
                                    GhostOwner = Selected;
                                    break;
                                }
                            }

                            TWeakObjectPtr<AExtendedControllerBase> WeakController(ExtendedController);
                            TWeakObjectPtr<AUnitBase> WeakWorker(GhostOwner);
                            const FTransform DropTransform = GetActorTransform();

                            if (UWorld* World = GetWorld())
                            {
                                World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
                                    [WeakController, WeakWorker, DropTransform]()
                                    {
                                        if (!WeakController.IsValid() || !WeakWorker.IsValid()) return;
                                        WeakController->SetWorkArea(DropTransform);
                                        // Ohne Geist gibt es nichts abzuwerfen: dann hat die Faehigkeit
                                        // oben keine Baustelle erzeugt, der Druck war also schon vorher
                                        // wirkungslos. DropWorkAreaForUnit verlaesst diesen Fall auf
                                        // Verbose und damit unsichtbar - gemessen am 01.09.2026 blieben
                                        // 165 von 210 Fehlschlaegen ohne jeden benannten Grund.
                                        const bool bHatteGeist = WeakWorker->CurrentDraggedWorkArea != nullptr;
                                        const bool bAbwurfGelang = WeakController->DropWorkAreaForUnit(
                                            WeakWorker.Get(), false, WeakController->DropWorkAreaFailedSound);
                                        // Der Bauplatz ist die Kameraposition des Agenten. Scheitert der
                                        // Abwurf, war der Druck umsonst - bisher lautlos.
                                        UE_LOG(LogTemp, Warning,
                                            TEXT("[NetzDruck] Team %d Abwurf %s Geist=%d bei (%.0f,%.0f)"),
                                            WeakController->SelectableTeamId,
                                            bAbwurfGelang ? TEXT("OK") : TEXT("FEHLGESCHLAGEN"),
                                            bHatteGeist ? 1 : 0,
                                            DropTransform.GetLocation().X, DropTransform.GetLocation().Y);
                                    }));
                            }
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning,
                                TEXT("[NetzDruck] Team %d VERWORFEN: keine Einheit fuer den Abwurf ausgewaehlt"),
                                ExtendedController->SelectableTeamId);
                            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] switch_camera_state_ability: No unit selected to drop work area for."));
                        }
                    }
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
                    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Left Click: No ground hit found."));
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
                    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("[ARLAgent] Right Click: No ground hit found."));
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
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("JSON Error: %s"), *JsonReader->GetErrorMessage());
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
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase in PerformRightClickAction."));
        return;
    }

    ExtendedController->AttackToggled = false;

    // The agent's right-click traces straight DOWN from its camera, so it can only ever hit whatever
    // happens to sit directly underneath - it will practically never land on a transporter. A human
    // aims at the Antimatter reactor and the workers get loaded; the agent could not, so that resource
    // source stayed unused. Re-aim the click at a nearby friendly transporter when the selection is
    // made of loadable workers. AI-only by construction: this whole class is the agent's input path.
    FHitResult EffectiveHit = HitResult;
    {
        bool bHasLoadableWorker = false;
        for (AUnitBase* Selected : ExtendedController->SelectedUnits)
        {
            // CanBeTransported is the real condition - requiring IsWorker as well meant the re-aim
            // only ever worked for workers, so a rule that sends SOLDIERS into a bunker right-clicked
            // wherever the camera happened to point and nothing was ever loaded.
            if (IsValid(Selected) && Selected->CanBeTransported &&
                Selected->GetUnitState() != UnitData::Dead)
            {
                bHasLoadableWorker = true;
                break;
            }
        }

        if (bActionAimsAtTransporter)
        {
            // Logged BEFORE the guards, not after - a log inside the block only ever fires on success
            // and tells nothing about which condition rejected the click.
            UE_LOG(LogTemp, Warning, TEXT("[Load] aim=1 selected=%d loadable=%d hitActorIsUnit=%d"),
                   ExtendedController->SelectedUnits.Num(), bHasLoadableWorker ? 1 : 0,
                   Cast<AUnitBase>(HitResult.GetActor()) ? 1 : 0);
        }

        if (bHasLoadableWorker && !Cast<AUnitBase>(HitResult.GetActor()))
        {
            const FVector ClickLocation = HitResult.Location;
            AUnitBase* BestTransporter = nullptr;
            double BestDistSq = TNumericLimits<double>::Max();

            for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
            {
                AUnitBase* Candidate = *It;
                if (!IsValid(Candidate) || !Candidate->IsATransporter) continue;
                if (Candidate->TeamId != ExtendedController->SelectableTeamId) continue;
                if (Candidate->GetUnitState() == UnitData::Dead) continue;
                // Full transporters are skipped, otherwise the workers walk over and bounce off.
                if (Candidate->CurrentUnitsLoaded >= Candidate->MaxTransportUnits) continue;

                const double DistSq = FVector::DistSquared2D(Candidate->GetActorLocation(), ClickLocation);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestTransporter = Candidate;
                }
            }

            // A rule that explicitly means "load these units" targets the transporter at any distance.
            // The radius only guards the accidental case, where the agent happens to right-click near
            // one while giving an ordinary move order - that ambiguity is exactly what the flag removes.
            if (bActionAimsAtTransporter)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Load] transporterFound=%d dist=%.0f"),
                       BestTransporter ? 1 : 0,
                       BestTransporter ? FMath::Sqrt(BestDistSq) : -1.0);
            }

            const float EffectiveRadius = bActionAimsAtTransporter ? TNumericLimits<float>::Max()
                                                                   : AiTransporterClickRadius;
            if (BestTransporter && (bActionAimsAtTransporter || BestDistSq <= FMath::Square(EffectiveRadius)))
            {
                EffectiveHit.Location = BestTransporter->GetActorLocation();
                EffectiveHit.ImpactPoint = EffectiveHit.Location;
                EffectiveHit.HitObjectHandle = FActorInstanceHandle(BestTransporter);

                // When the rule explicitly means "load", hand the transporter over DIRECTLY instead of
                // routing it through the hit result. CheckClickOnTransportUnit relies on
                // Hit_Pawn.GetActor() casting to AUnitBase, and a hand-built FHitResult does not
                // reliably reproduce that - the load then fails silently with no log at all. We already
                // hold the pointer here, so there is nothing to reconstruct.
                if (bActionAimsAtTransporter)
                {
                    // LoadUnits() only loads on the spot within InstantLoadRange - anything further away
                    // merely gets a run order and loads on arrival. That never completes for the AI: it
                    // re-decides every few seconds and re-selects the same tag group, so the walking unit
                    // is pulled away again long before it gets there. Measured result was Bunker [0,0,0]
                    // although the order itself went through every time. Load directly instead; nothing in
                    // C++ unloads again except the transporter's death (KillLoadedUnits).
                    int32 Attempted = 0;
                    int32 Loaded = 0;
                    for (AUnitBase* UnitToLoad : ExtendedController->SelectedUnits)
                    {
                        if (!IsValid(UnitToLoad) || UnitToLoad == BestTransporter) continue;
                        if (!UnitToLoad->CanBeTransported) continue;
                        if (UnitToLoad->GetUnitState() == UnitData::Dead) continue;

                        ++Attempted;
                        const int32 Before = BestTransporter->CurrentUnitsLoaded;
                        BestTransporter->LoadUnit(UnitToLoad);
                        if (BestTransporter->CurrentUnitsLoaded > Before) ++Loaded;
                    }

                    UE_LOG(LogTemp, Warning,
                           TEXT("[Load] team %d -> %s: attempted=%d loaded=%d now=%d/%d"),
                           ExtendedController->SelectableTeamId, *BestTransporter->GetName(),
                           Attempted, Loaded,
                           BestTransporter->CurrentUnitsLoaded, BestTransporter->MaxTransportUnits);
                    return;
                }
            }
        }
    }

    if (!ExtendedController->CheckClickOnTransportUnit(EffectiveHit))
    {
        if (ExtendedController->SelectedUnits.Num() == 0 ||
            (ExtendedController->SelectedUnits[0] && !ExtendedController->SelectedUnits[0]->CurrentDraggedWorkArea))
        {
            if (!ExtendedController->CheckClickOnWorkArea(EffectiveHit))
            {
                // Arbeiter aus dem Marschbefehl herausnehmen - aber NUR hier, im KI-Pfad.
                //
                // RunUnitsAndSetWaypointsMass laeuft ueber die gesamte Auswahl und kennt keinen
                // Arbeiterausschluss; PerformLeftClickAction hat ihn in beiden Zweigen. Der
                // Rechtsklick zieht deshalb die Arbeiter mit, und StopWorkOnSelectedUnit nimmt
                // ihnen dabei die Arbeit ab - genau das Bild "die KI schickt alle Arbeiter weg".
                // Ausgeloest wird es von Regelzeilen, die Aktion 29 (RightClick1) tragen; in der
                // Singularianer-Tabelle tut das Load_Antimatter.
                //
                // Der Ausschluss darf NICHT in RunUnitsAndSetWaypointsMass selbst stehen: dort
                // laeuft auch der Rechtsklick des Spielers (CustomControllerBase.cpp:2167) und der
                // Minimap-Befehl (:2202) durch. Ein Filter dort haette dem Spieler die Kontrolle
                // ueber seine Arbeiter genommen.
                TArray<AUnitBase*> AuswahlVorher = ExtendedController->SelectedUnits;
                ExtendedController->SelectedUnits.RemoveAll([](const AUnitBase* U)
                {
                    return U && U->IsWorker;
                });

                const int32 Entfernt = AuswahlVorher.Num() - ExtendedController->SelectedUnits.Num();
                if (Entfernt > 0)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ArbeiterSchutz] Team %d: %d von %d Arbeitern vom Marschbefehl ausgenommen"),
                        ExtendedController->SelectableTeamId, Entfernt, AuswahlVorher.Num());
                }

                if (ExtendedController->SelectedUnits.Num() > 0)
                {
                    ExtendedController->RunUnitsAndSetWaypointsMass(EffectiveHit);
                }

                ExtendedController->SelectedUnits = AuswahlVorher;
            }
        }
    }
 
    if (ExtendedController->SelectedUnits.Num() > 0 && ExtendedController->SelectedUnits[0] && ExtendedController->SelectedUnits[0]->CurrentDraggedWorkArea)
    {
        ExtendedController->DestroyDraggedArea(ExtendedController->SelectedUnits[0]);
    }
    
}

void ARLAgent::RunUnitsAndSetWaypoints(FHitResult Hit, AExtendedControllerBase* ExtendedController)
{
	// DEAD PATH, kept only so an existing reference still links: the sole call site (see
	// PerformMoveAction, where it sits commented out just below the live call) has been disabled -
	// RL move orders go through ACustomControllerBase::RunUnitsAndSetWaypointsMass.
	//
	// It used to carry its own copy of the layout logic built on ComputeGridSize +
	// CalculateGridOffset, which always produced a rectangle and silently ignored
	// GridFormationShape. Forwarding instead of maintaining that duplicate means re-enabling this
	// cannot resurrect the old mismatch between the RL path and the player path.
	if (ACustomControllerBase* CustomController = Cast<ACustomControllerBase>(ExtendedController))
	{
		CustomController->RunUnitsAndSetWaypointsMass(Hit);
	}
}

void ARLAgent::PerformLeftClickAction(const FHitResult& HitResult, bool AttackToggled)
{
    
    ACustomControllerBase* CustomControllerBase = Cast<ACustomControllerBase>(GetController());
    if (!CustomControllerBase)
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("[ARLAgent] Could not cast Controller to AExtendedControllerBase in PerformLeftClickAction."));
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
        AWaypoint* BWaypoint = nullptr;

        // Der Angriffsbefehl der KI zielte bisher auf den Boden UNTER DEM AGENTEN - also dorthin, wo
        // die KI-Kamera gerade steht. Fuer die Regel-KI ist das folgenlos, sie geht ueber
        // IssueDirectAttackMove mit eigener Zielwahl. Fuer das NETZ war es der Grund, warum es nie
        // angreift: es muesste erst die Kamera an den Feind fahren und dann angreifen, und diese
        // Verkettung gelingt ihm praktisch nie. Gemessen ueber 34 Partien: Team 1 (Netz) NULL
        // Angriffsbefehle, Team 2 (Regeln) bis zu 90 - bei besserer Bauleistung des Netzes.
        //
        // Deshalb bekommt der Angriffsbefehl hier dieselbe Zielwahl wie die Regel-KI: das naechste
        // gegnerische GEBAEUDE zum eigenen Schwerpunkt. Gebaeude, weil sie stehen bleiben (siehe
        // bPreferBuildingTargets im Entscheider). Findet sich keines, bleibt es beim urspruenglichen
        // Punkt, das Verhalten ist dann wie zuvor.
        //
        // Dieser Zweig liegt in ARLAgent und wird ausschliesslich von der KI durchlaufen - der
        // Spieler ist nicht betroffen.
        FHitResult ZielTreffer = HitResult;
        if (bAimAttackAtNearestEnemyBuilding)
        {
            if (const UWorld* Welt = GetWorld())
            {
                const int32 EigenesTeam = CustomControllerBase->SelectableTeamId;

                FVector EigenerSchwerpunkt = FVector::ZeroVector;
                int32 EigeneZahl = 0;
                FVector BestesZiel = FVector::ZeroVector;
                bool bZielGefunden = false;

                TArray<ABuildingBase*> Gegnerische;
                for (TActorIterator<ABuildingBase> It(Welt); It; ++It)
                {
                    ABuildingBase* Gebaeude = *It;
                    if (!IsValid(Gebaeude)) continue;
                    if (Gebaeude->TeamId == EigenesTeam)
                    {
                        EigenerSchwerpunkt += Gebaeude->GetActorLocation();
                        ++EigeneZahl;
                    }
                    else
                    {
                        Gegnerische.Add(Gebaeude);
                    }
                }

                const FVector Bezug = (EigeneZahl > 0)
                    ? (EigenerSchwerpunkt / (float)EigeneZahl)
                    : GetActorLocation();

                double BesteDistSq = TNumericLimits<double>::Max();
                for (const ABuildingBase* Gegner : Gegnerische)
                {
                    const double DistSq = FVector::DistSquared2D(Gegner->GetActorLocation(), Bezug);
                    if (DistSq < BesteDistSq)
                    {
                        BesteDistSq = DistSq;
                        BestesZiel = Gegner->GetActorLocation();
                        bZielGefunden = true;
                    }
                }

                if (bZielGefunden)
                {
                    ZielTreffer.Location = BestesZiel;
                    ZielTreffer.ImpactPoint = BestesZiel;
                    UE_LOG(LogTemp, Warning,
                        TEXT("[NetzAngriff] Team=%d Angriffsziel auf naechstes Gegnergebaeude gesetzt: (%.0f, %.0f), %d Kandidaten"),
                        EigenesTeam, BestesZiel.X, BestesZiel.Y, Gegnerische.Num());
                }
            }
        }
        const FHitResult& HitResultZiel = ZielTreffer;

        // Use the same formation solver the human paths use, so GridFormationShape means the same
        // thing for an RL agent as for a player. The ring and wedge layouts size each ring from the
        // largest unit still unplaced, so they REQUIRE descending-radius order - hence the sorted
        // copy, mirroring HandleAttackMovePressed. Offsets are looked up per unit so the loop below
        // (and which units get FireAbilityMouseHit) stays exactly as it was.
        TMap<AUnitBase*, FVector> RLFormationOffsets;
        {
            TArray<AUnitBase*> SortedUnits;
            for (AUnitBase* Candidate : CustomControllerBase->SelectedUnits)
            {
                if (Candidate && Candidate != CustomControllerBase->CameraUnitWithTag
                    && !Candidate->IsWorker && !Cast<ABuildingBase>(Candidate))
                {
                    SortedUnits.Add(Candidate);
                }
            }
            SortedUnits.Sort([](const AUnitBase& A, const AUnitBase& B)
            {
                float RA = 50.0f;
                if (A.GetCapsuleComponent()) RA = A.GetCapsuleComponent()->GetScaledCapsuleRadius();
                float RB = 50.0f;
                if (B.GetCapsuleComponent()) RB = B.GetCapsuleComponent()->GetScaledCapsuleRadius();
                if (FMath::IsNearlyEqual(RA, RB)) return A.GetName() > B.GetName();
                return RA > RB;
            });

            const TArray<FVector> Offsets = CustomControllerBase->ComputeSlotOffsetsDirectional(
                SortedUnits, -1.f,
                CustomControllerBase->ComputeApproachDirection(SortedUnits, HitResultZiel.Location));

            for (int32 s = 0; s < SortedUnits.Num() && s < Offsets.Num(); ++s)
            {
                RLFormationOffsets.Add(SortedUnits[s], Offsets[s]);
            }
        }

        // Collect all mass units and their positions to issue one RPC
        TArray<AUnitBase*> MassUnits;
        TArray<FVector>    MassLocations;
        TArray<AUnitBase*> BuildingUnits;
        TArray<FVector>    BuildingLocs;
        bool PlayWaypointSoundTotal = false;

        for (int32 i = 0; i < CustomControllerBase->SelectedUnits.Num(); i++)
        {
            AUnitBase* U = CustomControllerBase->SelectedUnits[i];
            if (U != CustomControllerBase->CameraUnitWithTag && !U->IsWorker)
            {
                // FindRef yields a zero offset for buildings (never inserted), which is exactly the
                // "use the raw hit location" case the ternary already handled.
                FVector RunLocation = Cast<ABuildingBase>(U) ? (FVector)HitResultZiel.Location : (FVector)HitResultZiel.Location + RLFormationOffsets.FindRef(U);
                bool HitNavModifier;
                RunLocation = CustomControllerBase->TraceRunLocation(RunLocation, HitNavModifier);
                if (HitNavModifier) continue;

                bool PlayWaypointSound;
                
                bool bSuccess = false;
                CustomControllerBase->SetBuildingWaypoint(RunLocation, U, BWaypoint, PlayWaypointSound, bSuccess);
                if (bSuccess)
                {
                    // Do Nothing
                    BuildingUnits.Add(U);
                    BuildingLocs.Add(RunLocation);
                    if (PlayWaypointSound) PlayWaypointSoundTotal = true;
                }
                else
                {
                    CustomControllerBase->DrawCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
                    if (U->bIsMassUnit)
                    {
                        MassUnits.Add(U);
                        MassLocations.Add(RunLocation);
                    }
                    else
                    {
                        CustomControllerBase->LeftClickAttack(U, RunLocation);
                    }
                }
            }

            if (U)
                CustomControllerBase->FireAbilityMouseHit(U, HitResultZiel);
        }

        if (BuildingUnits.Num() > 0)
        {
            CustomControllerBase->Server_Batch_SetBuildingWaypoints(BuildingLocs, BuildingUnits);
        }

        if (CustomControllerBase->WaypointSound && PlayWaypointSoundTotal)
        {
            UGameplayStatics::PlaySound2D(CustomControllerBase, CustomControllerBase->WaypointSound, CustomControllerBase->GetSoundMultiplier());
        }

        if (MassUnits.Num() > 0)
        {
            CustomControllerBase->LeftClickAttackMass(MassUnits, MassLocations, AttackToggled);
        }

    } else {
        for (int32 i = 0; i < CustomControllerBase->SelectedUnits.Num(); i++)
        {
            if (CustomControllerBase->SelectedUnits[i] && !CustomControllerBase->SelectedUnits[i]->IsWorker && CustomControllerBase->SelectedUnits[i]->CurrentSnapshot.AbilityClass && CustomControllerBase->CurrentDraggedAbilityIndicator)
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
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("ARLAgent::Server_PlayGame_Implementation --- Called for Team ID: %d ---"), SelectableTeamId);

    // 1. Get the current game state (you need to implement this part)
    FGameStateData GameState = GatherGameState(SelectableTeamId);
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("Step 1/3: Game state gathered."));

    // 2. Get the action JSON from the inference component
    FString ActionJSON = InferenceComponent->ChooseJsonAction(GameState);

    // Log the action chosen by the model. This is the most important log.
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("Step 2/3: Inference component returned ActionJSON: %s"), *ActionJSON);

    // 3. Process the action using your existing function
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("Step 3/3: Passing ActionJSON to ReceiveRLAction for processing."));
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
    // If Shared Memory IO is disabled, do nothing in this path (BT mode or non-RL usage)
    if (!bEnableSharedMemoryIO)
    {
        return;
    }

    // Convert GameStateData to JSON (or another format your RL process understands)
    FString GameStateJSON = CreateGameStateJSON(GameState);
    
    // Write to shared memory
    if (SharedMemoryManager)
    {
        SharedMemoryManager->WriteGameState(GameStateJSON);
    }
    else
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("SharedMemoryManager is not valid, cannot write game state."));
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

    // Pre-resolve the gameplay tags we care about
    static const FGameplayTag TagAlt1 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Alt1"));
    static const FGameplayTag TagAlt2 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Alt2"));
    static const FGameplayTag TagAlt3 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Alt3"));
    static const FGameplayTag TagAlt4 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Alt4"));
    static const FGameplayTag TagAlt5 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Alt5"));
    static const FGameplayTag TagAlt6 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Alt6"));

    static const FGameplayTag TagCtrl1 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Ctrl1"));
    static const FGameplayTag TagCtrl2 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Ctrl2"));
    static const FGameplayTag TagCtrl3 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Ctrl3"));
    static const FGameplayTag TagCtrl4 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Ctrl4"));
    static const FGameplayTag TagCtrl5 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Ctrl5"));
    static const FGameplayTag TagCtrl6 = FGameplayTag::RequestGameplayTag(FName("KeyTag.Ctrl6"));

    static const FGameplayTag TagCtrlQ = FGameplayTag::RequestGameplayTag(FName("KeyTag.CtrlQ"));
    static const FGameplayTag TagCtrlW = FGameplayTag::RequestGameplayTag(FName("KeyTag.CtrlW"));
    static const FGameplayTag TagCtrlE = FGameplayTag::RequestGameplayTag(FName("KeyTag.CtrlE"));
    static const FGameplayTag TagCtrlR = FGameplayTag::RequestGameplayTag(FName("KeyTag.CtrlR"));

    for (AActor* Unit : GameMode->AllUnits)
    {
        AUnitBase* MyUnit = Cast<AUnitBase>(Unit);

        if (MyUnit && MyUnit->IsValidLowLevelFast()) // Ensure the unit is valid
        {
            UAttributeSetBase* Attributes = MyUnit->Attributes;
            if (Attributes && Attributes->IsValidLowLevelFast()) // Ensure attributes are valid
            {
                const bool bFriendly = (MyUnit->TeamId == SelectableTeamId);
                if (bFriendly)
                {
                    GameState.MyUnitCount++;
                    GameState.MyTotalHealth += Attributes->GetHealth();
                    GameState.MyTotalAttackDamage += Attributes->GetAttackDamage();
                    SumFriendlyPositions += MyUnit->GetMassActorLocation();
                    NumFriendlyUnits++;
                }
                else
                {
                    GameState.EnemyUnitCount++;
                    GameState.EnemyTotalHealth += Attributes->GetHealth();
                    GameState.EnemyTotalAttackDamage += Attributes->GetAttackDamage();
                    SumEnemyPositions += MyUnit->GetMassActorLocation();
                    NumEnemyUnits++;
                }

                // Count per-tag unit membership
                auto CountTag = [&MyUnit, bFriendly](const FGameplayTag& T, int32& FriendlyCounter, int32& EnemyCounter)
                {
                    if (T.IsValid() && MyUnit->UnitTags.HasTagExact(T))
                    {
                        if (bFriendly) { ++FriendlyCounter; } else { ++EnemyCounter; }
                    }
                };

                CountTag(TagAlt1, GameState.Alt1TagFriendlyUnitCount, GameState.Alt1TagEnemyUnitCount);
                CountTag(TagAlt2, GameState.Alt2TagFriendlyUnitCount, GameState.Alt2TagEnemyUnitCount);
                CountTag(TagAlt3, GameState.Alt3TagFriendlyUnitCount, GameState.Alt3TagEnemyUnitCount);
                CountTag(TagAlt4, GameState.Alt4TagFriendlyUnitCount, GameState.Alt4TagEnemyUnitCount);
                CountTag(TagAlt5, GameState.Alt5TagFriendlyUnitCount, GameState.Alt5TagEnemyUnitCount);
                CountTag(TagAlt6, GameState.Alt6TagFriendlyUnitCount, GameState.Alt6TagEnemyUnitCount);

                CountTag(TagCtrl1, GameState.Ctrl1TagFriendlyUnitCount, GameState.Ctrl1TagEnemyUnitCount);
                CountTag(TagCtrl2, GameState.Ctrl2TagFriendlyUnitCount, GameState.Ctrl2TagEnemyUnitCount);
                CountTag(TagCtrl3, GameState.Ctrl3TagFriendlyUnitCount, GameState.Ctrl3TagEnemyUnitCount);
                CountTag(TagCtrl4, GameState.Ctrl4TagFriendlyUnitCount, GameState.Ctrl4TagEnemyUnitCount);
                CountTag(TagCtrl5, GameState.Ctrl5TagFriendlyUnitCount, GameState.Ctrl5TagEnemyUnitCount);
                CountTag(TagCtrl6, GameState.Ctrl6TagFriendlyUnitCount, GameState.Ctrl6TagEnemyUnitCount);

                CountTag(TagCtrlQ, GameState.CtrlQTagFriendlyUnitCount, GameState.CtrlQTagEnemyUnitCount);
                CountTag(TagCtrlW, GameState.CtrlWTagFriendlyUnitCount, GameState.CtrlWTagEnemyUnitCount);
                CountTag(TagCtrlE, GameState.CtrlETagFriendlyUnitCount, GameState.CtrlETagEnemyUnitCount);
                CountTag(TagCtrlR, GameState.CtrlRTagFriendlyUnitCount, GameState.CtrlRTagEnemyUnitCount);
            }
        }
    }
    
    // Spielzeit und Bauwarteschlange - die beiden Innenzustaende, die der Regel-Lehrer abfragt und
    // die im Zustandsvektor bisher fehlten. Begruendung an den Feldern in FGameStateData.
    if (UWorld* StateWorld = GetWorld())
    {
        GameState.GameTimeSeconds = StateWorld->GetTimeSeconds();

        // Geplante, noch nicht fertige Bauten. Gezaehlt wird ueber die Gebaeudeklasse der Flaeche -
        // genau so, wie CountByClassTag es mit bIncludePendingAreas=true tut. Eine Flaeche, deren
        // Gebaeude schon steht, ist keine Warteschlange mehr und faellt ueber Building heraus.
        auto ZaehlePending = [](const FGameplayTag& T, const AUnitBase* CDO, int32& Counter)
        {
            if (T.IsValid() && CDO && CDO->UnitTags.HasTagExact(T))
            {
                ++Counter;
            }
        };

        for (TActorIterator<AWorkArea> It(StateWorld); It; ++It)
        {
            const AWorkArea* Area = *It;
            if (!IsValid(Area) || Area->Type != WorkAreaData::BuildArea) continue;
            if (Area->TeamId != SelectableTeamId) continue;
            if (Area->Building || Area->bFinalBuildingSpawned) continue;   // schon gebaut
            if (!Area->BuildingClass) continue;

            const AUnitBase* BuildingCDO = Cast<AUnitBase>(Area->BuildingClass->GetDefaultObject());
            if (!BuildingCDO) continue;

            ZaehlePending(TagAlt1, BuildingCDO, GameState.Alt1TagPendingBuildCount);
            ZaehlePending(TagAlt2, BuildingCDO, GameState.Alt2TagPendingBuildCount);
            ZaehlePending(TagAlt3, BuildingCDO, GameState.Alt3TagPendingBuildCount);
            ZaehlePending(TagAlt4, BuildingCDO, GameState.Alt4TagPendingBuildCount);
            ZaehlePending(TagAlt5, BuildingCDO, GameState.Alt5TagPendingBuildCount);
            ZaehlePending(TagAlt6, BuildingCDO, GameState.Alt6TagPendingBuildCount);
            ZaehlePending(TagCtrl1, BuildingCDO, GameState.Ctrl1TagPendingBuildCount);
            ZaehlePending(TagCtrl2, BuildingCDO, GameState.Ctrl2TagPendingBuildCount);
            ZaehlePending(TagCtrl3, BuildingCDO, GameState.Ctrl3TagPendingBuildCount);
            ZaehlePending(TagCtrl4, BuildingCDO, GameState.Ctrl4TagPendingBuildCount);
            ZaehlePending(TagCtrl5, BuildingCDO, GameState.Ctrl5TagPendingBuildCount);
            ZaehlePending(TagCtrl6, BuildingCDO, GameState.Ctrl6TagPendingBuildCount);
            ZaehlePending(TagCtrlQ, BuildingCDO, GameState.CtrlQTagPendingBuildCount);
            ZaehlePending(TagCtrlW, BuildingCDO, GameState.CtrlWTagPendingBuildCount);
            ZaehlePending(TagCtrlE, BuildingCDO, GameState.CtrlETagPendingBuildCount);
            ZaehlePending(TagCtrlR, BuildingCDO, GameState.CtrlRTagPendingBuildCount);
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

        GameState.MaxPrimaryResource = GameMode->GetMaxResource(EResourceType::Primary, MyTeamId);
        GameState.MaxSecondaryResource = GameMode->GetMaxResource(EResourceType::Secondary, MyTeamId);
        GameState.MaxTertiaryResource = GameMode->GetMaxResource(EResourceType::Tertiary, MyTeamId);
        GameState.MaxRareResource = GameMode->GetMaxResource(EResourceType::Rare, MyTeamId);
        GameState.MaxEpicResource = GameMode->GetMaxResource(EResourceType::Epic, MyTeamId);
        GameState.MaxLegendaryResource = GameMode->GetMaxResource(EResourceType::Legendary, MyTeamId);
    }

    return GameState;
}