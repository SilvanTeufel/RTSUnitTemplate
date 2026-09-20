// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Camera/RL/InferenceComponent.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h" // Important: Include the specific CPU runtime header
#include "NNETypes.h"
#include "NNERuntimeRunSync.h"     // <-- ADD THIS for FTensorBinding
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Characters/Camera/RLAgent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Characters/Unit/UnitBase.h"
#include "HAL/IConsoleManager.h"

// Ein Faehigkeitsdruck auf einen Slot, den keine der ausgewaehlten Einheiten belegt, ist ein
// reiner Leerzug: die Aktivierung laeuft durch, es entsteht aber keine Baustelle. Gemessen am
// 01.09.2026 im Fenster 0-60 s: Taste 1 zu 97 % und Taste 5 zu 61 % ohne Baustelle, insgesamt
// 209 von 522 Druecken verschenkt - waehrend die Regel-KI sich auf die zwei passenden Tasten
// beschraenkt und dort auf 0 % kommt. Das Netz hat die Randhaeufigkeit der Tasten gelernt,
// nicht die Bedingung "welche Gruppe ist gerade gewaehlt".
// 1 = solche Aktionen bei der Auswahl ausblenden. Betrifft nur den KI-Pfad.
// 2 = zusaetzlich unbezahlbare Bauaktionen ausblenden.
// 3 = leere Gruppenwahlen ausblenden (Anlauf 4, 02.09.2026).
// 4 = den Zweitakt "waehlen, feuern" erzwingen (Anlauf 5, 19.09.2026) - siehe IstAktionMoeglich.
static int32 GRLActionMask = 0;
static FAutoConsoleVariableRef CVarRLActionMask(
    TEXT("rts.rl.action.mask"),
    GRLActionMask,
    TEXT("0 = aus. 1 = Slot ohne Bau-Faehigkeit ausblenden (Anlauf 2, gemessen wirkungslos). ")
    TEXT("2 = unbezahlbare Bau-Faehigkeiten ausblenden (Anlauf 3: Bauquote 53->100 %, Ergebnis unveraendert). ")
    TEXT("3 = leere Gruppenwahlen ausblenden (Anlauf 4, zielt auf die gemessene PPO-Drift). Nur KI-Pfad."),
    ECVF_Default);
#include "GameFramework/Pawn.h"
#include "GameModes/ResourceGameMode.h"
#include "EngineUtils.h"

// Bring the NNE namespace into scope to simplify type names
using namespace UE::NNE;

namespace
{
    /**
     * Softmax temperature for picking an action from the network's scores.
     * 0 = greedy argmax. A behaviour-cloned policy needs sampling: it was trained to reproduce a mixture of
     * actions per state, and argmax keeps only the single most likely one, which in practice means the
     * agent repeats its most common key and never performs the rarer, decisive ones.
     * 1.0 reproduces the learned distribution; lower is more decisive, higher more erratic.
     *
     * 0.4 is measured, not guessed. Four values, 36 clean matches each against the rule-based AI,
     * paired by seed (win rate / mean unit difference):
     *
     *     1.0 -> 19 % / -7.9      0.7 -> 44 % / -1.6
     *     0.4 -> 56 % / -0.1      0.15 -> 17 % / -4.3
     *
     * The optimum is interior and both ends are significantly worse (p = 0.002 and p = 0.0006);
     * 0.4 against 0.7 is not separable (p = 0.35), so treat 0.4 to 0.7 as a plateau rather than
     * a sharp peak. Both extremes hurt for different reasons: too much spread dilutes the choice,
     * too little collapses the policy onto its most common action - the failure described above.
     *
     * Measured on the clean matches only. In a parallel measurement run one instance runs roughly
     * 15x the frame rate of the others and therefore gets 15x the AI decisions, which the
     * rule-based side exploits far better than the network; those matches are not valid samples.
     */
    static float GRLSamplingTemperature = 0.4f;
    static FAutoConsoleVariableRef CVarRLSamplingTemperature(
        TEXT("rts.ai.rl.temperature"),
        GRLSamplingTemperature,
        TEXT("Softmax temperature for RL action selection. 0 = greedy argmax, 1 = as trained."),
        ECVF_Default);
}

float UInferenceComponent::GetSamplingTemperature()
{
    return GRLSamplingTemperature;
}

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

    // Only initialize the RL model when running in RL mode. In BT mode we skip this to avoid noise and unnecessary setup.
    // GetEffectiveBrainMode statt BrainMode: sonst meldet die Zeile "Behavior_Tree", obwohl ein
    // Team-Override oder der Kommandozeilenschalter laengst auf das Netz zeigt - das hat beim
    // Selbstspiel-Aufbau eine Fehlersuche gekostet. Gebaut wird das Modell ohnehin erst spaeter.
    if (GetEffectiveBrainMode() != EBrainMode::RL_Model)
    {
        UE_LOG(LogTemp, Log, TEXT("InferenceComponent: Skipping RL model init (BrainMode is Behavior_Tree)."));
        return;
    }

    // The model itself is built lazily: the pawn is possessed after BeginPlay, so the team id needed to
    // pick a per-team model is not known yet.
}

namespace
{
    /**
     * Hirnmodus je Team per Kommandozeile - fuer den unbeaufsichtigten Selbstspiel-Lauf
     * (16.08.2026). Format: "1:1,2:0" (TeamId:Modus, 1 = trainiertes Netz, 0 = Regel-KI).
     *
     * Warum ein CVar und nicht das vorhandene rts.rl.brain-Kommando: -ExecCmds laeuft ERST NACH
     * dem Laden der Karte. Zu dem Zeitpunkt hat die InferenceComponent ihr Modell schon
     * uebersprungen ("Skipping RL model init (BrainMode is Behavior_Tree)") und die KI-Pawns
     * existierten beim Ausfuehren teils noch gar nicht ("Team 1 - no AI"). Ueber -dpcvars=
     * steht der Wert dagegen vor jeder Weltinitialisierung fest.
     */
    static FString GRLBrainOverrides;
    static FAutoConsoleVariableRef CVarRLBrainOverrides(
        TEXT("rts.rl.brain.teams"),
        GRLBrainOverrides,
        TEXT("Hirnmodus je Team, z.B. \"1:1,2:0\" (1 = trainiertes Netz, 0 = Regel-KI)."),
        ECVF_Default);

    /**
     * Sampling-Temperatur je Team - gemessen am 19./20.09.2026.
     *
     * Die Temperatur gehoert zum LEHRER, nicht zum Spiel. Der Xeno-Lehrer spielt eine echte
     * Mischung; greedy liess den Agenten dort auf eine einzige Taste zusammenfallen, deshalb
     * steht die globale Vorgabe auf 0,4. Der Singularianer-Lehrer spielt dagegen fast
     * deterministisch: ueber 290 454 Entscheidungen folgt nach einer Gruppenwahl zu 91,3 % eine
     * Faehigkeit, sonst zu 0,0 %. Wuerfeln zerstoert diese Politik, statt sie aufzulockern.
     *
     * Gemessen gegen PPO 33, je n = 36: Temperatur 0,4 ergab alive -68,92 (SE 8,79),
     * Temperatur 0 ergab -22,36 (SE 2,34) - eine Differenz von +46,56 bei t = 5,12, ohne dass
     * am Netz irgendetwas geaendert wurde.
     *
     * Solange eine einzige Zahl fuer beide Fraktionen gilt, misst jede gemeinsame Reihe eine der
     * beiden falsch. Format wie beim Hirnmodus: "1:0.4|2:0".
     */
    static FString GRLTemperatureOverrides;
    static FAutoConsoleVariableRef CVarRLTemperatureOverrides(
        TEXT("rts.ai.rl.temperature.teams"),
        GRLTemperatureOverrides,
        TEXT("Sampling-Temperatur je Team, z.B. \"1:0.4|2:0\". Leer = globaler Wert."),
        ECVF_Default);

    /** Liefert true und die Temperatur, wenn fuer dieses Team eine gesetzt ist. */
    bool FindTemperatureOverride(int32 TeamId, float& OutTemperature)
    {
        if (GRLTemperatureOverrides.IsEmpty() || TeamId < 0)
        {
            return false;
        }
        FString Normalisiert = GRLTemperatureOverrides.Replace(TEXT("|"), TEXT(","));
        TArray<FString> Pairs;
        Normalisiert.ParseIntoArray(Pairs, TEXT(","), true);
        for (const FString& Pair : Pairs)
        {
            FString Left, Right;
            if (!Pair.Split(TEXT(":"), &Left, &Right))
            {
                continue;
            }
            if (FCString::Atoi(*Left.TrimStartAndEnd()) == TeamId)
            {
                OutTemperature = FCString::Atof(*Right.TrimStartAndEnd());
                return true;
            }
        }
        return false;
    }

    /** Liefert true und den Modus, wenn fuer dieses Team ein Override gesetzt ist. */
    bool FindBrainOverride(int32 TeamId, EBrainMode& OutMode)
    {
        if (GRLBrainOverrides.IsEmpty() || TeamId < 0)
        {
            return false;
        }
        // '|' ebenfalls als Trenner zulassen: -dpcvars= trennt seine eigenen Eintraege mit Komma,
        // ein Komma IM Wert kaeme also gar nicht erst hier an.
        FString Normalisiert = GRLBrainOverrides.Replace(TEXT("|"), TEXT(","));
        TArray<FString> Pairs;
        Normalisiert.ParseIntoArray(Pairs, TEXT(","), true);
        for (const FString& Pair : Pairs)
        {
            FString Left, Right;
            if (!Pair.Split(TEXT(":"), &Left, &Right))
            {
                continue;
            }
            if (FCString::Atoi(*Left.TrimStartAndEnd()) == TeamId)
            {
                OutMode = FCString::Atoi(*Right.TrimStartAndEnd()) != 0
                    ? EBrainMode::RL_Model : EBrainMode::Behavior_Tree;
                return true;
            }
        }
        return false;
    }
}

EBrainMode UInferenceComponent::GetEffectiveBrainMode() const
{
    const int32 TeamId = ResolveOwningTeamId();

    // DIAGNOSE (16.08.2026): einmal je Komponente ausgeben, was hier ankommt - ohne das laesst sich
    // nicht unterscheiden, ob der Override leer ist oder das Team noch unbekannt.
    static TSet<const UInferenceComponent*> Gemeldet;
    if (!Gemeldet.Contains(this))
    {
        Gemeldet.Add(this);
        UE_LOG(LogTemp, Warning, TEXT("[BrainDiag] TeamId=%d Override='%s' TeamMapNum=%d Default=%d"),
               TeamId, *GRLBrainOverrides, TeamBrainMode.Num(), (int32)BrainMode);
    }

    EBrainMode Override = EBrainMode::Behavior_Tree;
    if (FindBrainOverride(TeamId, Override))
    {
        return Override;
    }

    if (TeamId >= 0)
    {
        if (const EBrainMode* Found = TeamBrainMode.Find(TeamId))
        {
            return *Found;
        }
    }
    return BrainMode;
}

int32 UInferenceComponent::ResolveOwningTeamId() const
{
    if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
    {
        if (const AController* C = OwnerPawn->GetController())
        {
            if (const AControllerBase* CB = Cast<AControllerBase>(C))
            {
                return CB->SelectableTeamId;
            }
        }
    }
    return -1;
}

void UInferenceComponent::EnsureModelInitialised()
{
    if (bModelInitialised)
    {
        return;
    }

    const int32 TeamId = ResolveOwningTeamId();
    if (TeamId < 0)
    {
        return; // Not possessed yet; try again on the next decision.
    }

    bModelInitialised = true;

    UNNEModelData* ModelData = QNetworkModelData;
    if (const TObjectPtr<UNNEModelData>* Found = TeamQNetworkModelData.Find(TeamId))
    {
        if (*Found)
        {
            ModelData = *Found;
        }
    }

    if (!ModelData)
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: No ONNX model for team %d (neither a per-team entry nor QNetworkModelData)."), TeamId);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("InferenceComponent: Team %d uses model '%s'."), TeamId, *GetNameSafe(ModelData));

    // Get the specific CPU runtime for ONNX
    TWeakInterfacePtr<INNERuntimeCPU> Runtime = GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));
    if (!Runtime.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: NNERuntimeORTCpu could not be found!"));
        return;
    }

    // Create a model compatible with the CPU
    RuntimeModel = Runtime->CreateModelCPU(ModelData);
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
    const FTensorShape InputShape = FTensorShape::Make({1, static_cast<uint32>(GetStateSize())});
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
    return StateToArray(GameStateData);
}

int32 UInferenceComponent::GetActionSpaceSize()
{
    // Built once on a throwaway instance so callers (recorder, trainer export, UI) cannot disagree with the
    // list in InitializeActionSpace.
    static const int32 CachedSize = []()
    {
        UInferenceComponent* Probe = NewObject<UInferenceComponent>(GetTransientPackage());
        Probe->InitializeActionSpace();
        return Probe->ActionSpace.Num();
    }();
    return CachedSize;
}

TArray<float> UInferenceComponent::StateToArray(const FGameStateData& GameStateData)
{
    TArray<float> StateArray;
    StateArray.Reserve(GetStateSize());

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

    // Per-hotkey friendly unit counts. Without these the vector says how many units exist but not *what*
    // they are, and every build decision in this game turns on exactly that: "do I own a production
    // building yet", "how many workers", "how many hives". Behaviour cloning against the rule AI could not
    // get past chance level while these were missing - the network simply could not see what the rules read.
    StateArray.Add(static_cast<float>(GameStateData.Ctrl1TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl2TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl3TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl4TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl5TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl6TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlQTagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlWTagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlETagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlRTagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt1TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt2TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt3TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt4TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt5TagFriendlyUnitCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt6TagFriendlyUnitCount));

    // What the agent just did. See FGameStateData::LastActionIndex - without it the two halves of a
    // select-then-use decision are indistinguishable and the data cannot be learned from.
    StateArray.Add(static_cast<float>(GameStateData.LastActionIndex));

    // Spielzeit und Bauwarteschlange - siehe die Begruendung an den Feldern in FGameStateData.
    // Beides liest der Lehrer nachweislich (GameTimeCap, CountByClassTag mit bIncludePendingAreas),
    // beides fehlte im Vektor. Anders als der zurueckgenommene 60-Werte-Versuch sind das keine
    // plausibel klingenden Groessen, sondern genau die Abfragen aus EvaluateRuleRow.
    StateArray.Add(GameStateData.GameTimeSeconds);

    StateArray.Add(static_cast<float>(GameStateData.Alt1TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt2TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt3TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt4TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt5TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Alt6TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl1TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl2TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl3TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl4TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl5TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.Ctrl6TagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlQTagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlWTagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlETagPendingBuildCount));
    StateArray.Add(static_cast<float>(GameStateData.CtrlRTagPendingBuildCount));

    check(StateArray.Num() == GetStateSize());
    return StateArray;
}

int32 UInferenceComponent::GetStateSize()
{
    // 21 original features + 16 per-hotkey friendly counts + the previous action. Changing this invalidates
    // every previously trained model, so bump it deliberately and retrain - a mismatch is not reported by
    // NNE, the network just reads garbage.
    //
    // 29.08.2026 (frueher am Tag): probeweise auf 60 erweitert (Ressourcen-Obergrenzen +
    // Gegner-Aufteilung je Hotkey) und wieder ZURUECKGENOMMEN. Die Uebereinstimmung mit dem Lehrer
    // blieb exakt gleich (54,5 %), im Spiel fiel der Median von 47,5 auf 30,5 ueber je 12 Partien.
    // Die Lehre daraus: eine groessere Eingabe hilft nicht, es muessen die Groessen sein, die der
    // Decider TATSAECHLICH abfragt.
    //
    // 29.08.2026 (Nacht): 38 -> 55. Zwei davon sind belegt, nicht vermutet:
    //   * Spielzeit  - jede Regelzeile hat ein GameTimeCap [Min, Max] (EvaluateRuleRow).
    //   * 16 Bauwarteschlangen-Zaehler - CountByClassTag zaehlt mit bIncludePendingAreas=true,
    //     also die schon beauftragten Flaechen. Das ist der Grund, warum der Lehrer nicht doppelt
    //     baut; ohne diese Zahl kann das Netz den Unterschied gar nicht sehen.
    // Erfolgskriterium ist zuerst die Uebereinstimmung im Training (kostet keine Partie): steigt
    // sie ueber 54,5 %, traegt der Weg. Erst dann im Spiel gegenmessen.
    return 55;
}

FString UInferenceComponent::GetActionAsJSON(int32 ActionIndex)
{
    if (!ActionSpace.IsValidIndex(ActionIndex))
    {
        UE_LOG(LogTemp, Verbose, TEXT("InferenceComponent: Invalid ActionIndex %d provided."), ActionIndex);
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

void UInferenceComponent::ExecuteActionFromJSON(const FString& Json)
{
    if (Json.IsEmpty())
    {
        UE_LOG(LogTemp, Verbose, TEXT("InferenceComponent::ExecuteActionFromJSON: Empty JSON string."));
        return;
    }

    ARLAgent* RLAgent = Cast<ARLAgent>(GetOwner());
    if (!RLAgent)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::ExecuteActionFromJSON: Owner is not an ARLAgent; cannot forward action."));
        return;
    }

    // Helper lambda to process one object by serializing and forwarding to RLAgent
    auto ProcessObject = [RLAgent](const TSharedPtr<FJsonObject>& Obj)
    {
        if (!Obj.IsValid()) return;
        
        // Serialize this object back to a compact JSON string
        FString OutJson;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

        // Forward to RLAgent for execution
        RLAgent->ReceiveRLAction(OutJson);
    };

    // Detect array vs object quickly
    if (Json.Len() == 0)
    {
        return;
    }
    const TCHAR FirstChar = Json[0];
    if (FirstChar == '[')
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Arr))
        {
            UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::ExecuteActionFromJSON: Failed to parse JSON array."));
            return;
        }
        for (const TSharedPtr<FJsonValue>& V : Arr)
        {
            ProcessObject(V.IsValid() ? V->AsObject() : nullptr);
        }
        return;
    }

    // Single object: we can forward directly without re-serializing, but we parse to validate
    TSharedPtr<FJsonObject> Obj;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::ExecuteActionFromJSON: Failed to parse JSON object."));
        return;
    }

    // Forward the validated object
    ProcessObject(Obj);
}

// ============================================================================
// ActionSpace index mapping (0..35)
// ----------------------------------------------------------------------------
// This maps each ActionIndex to the concrete action serialized by GetActionAsJSON.
// NOTE:
// - Valid indices are 0..35 (36 actions total). If you use an invalid index, GetActionAsJSON returns "{}" and logs an error.
// - The older comment that mentioned 0..31 was outdated; this is the authoritative list.
//
//  0:  switch_camera_state 18   (Ctrl+R)
//  1:  switch_camera_state 9    (Ctrl+Q)
//  2:  switch_camera_state 10   (Ctrl+E)
//  3:  switch_camera_state 1    (Ctrl+W)
//  4:  switch_camera_state 21   (Ctrl+1)
//  5:  switch_camera_state 22   (Ctrl+2)
//  6:  switch_camera_state 23   (Ctrl+3)
//  7:  switch_camera_state 24   (Ctrl+4)
//  8:  switch_camera_state 25   (Ctrl+5)
//  9:  switch_camera_state 26   (Ctrl+6)
// 10:  switch_camera_state_ability 21   (Use ability 1)
// 11:  switch_camera_state_ability 22   (Use ability 2)
// 12:  switch_camera_state_ability 23   (Use ability 3)
// 13:  switch_camera_state_ability 24   (Use ability 4)
// 14:  switch_camera_state_ability 25   (Use ability 5)
// 15:  switch_camera_state_ability 26   (Use ability 6)
// 16:  change_ability_index 13
// 17:  move_camera 1            (direction 1)
// 18:  move_camera 2            (direction 2)
// 19:  move_camera 3            (direction 3)
// 20:  move_camera 4            (direction 4)
// 21:  resource_management 1    (assign workers -> resource 1)
// 22:  resource_management 2
// 23:  resource_management 3
// 24:  resource_management 4
// 25:  resource_management 5
// 26:  resource_management 6
// 27:  left_click 1             (e.g., move)
// 28:  left_click 2             (e.g., attack)
// 29:  right_click 1
// 30:  switch_camera_state 21   (Alt+1)
// 31:  switch_camera_state 22   (Alt+2)
// 32:  switch_camera_state 23   (Alt+3)
// 33:  switch_camera_state 24   (Alt+4)
// 34:  switch_camera_state 25   (Alt+5)
// 35:  switch_camera_state 26   (Alt+6)
// ----------------------------------------------------------------------------
// Each action serializes to a JSON like:
// {
//   "type": "Control",
//   "input_value": 1.0,
//   "alt": false/true,
//   "ctrl": true/false,
//   "action": "<see above>",
//   "camera_state": <number above>
// }
// ============================================================================
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
        {"Control", 1.0f, false, false, "right_click", 1},

        // Alt + 1-6
        {"Control", 1.0f, true,  false, "switch_camera_state", 21},
        {"Control", 1.0f, true,  false, "switch_camera_state", 22},
        {"Control", 1.0f, true,  false, "switch_camera_state", 23},
        {"Control", 1.0f, true,  false, "switch_camera_state", 24},
        {"Control", 1.0f, true,  false, "switch_camera_state", 25},
        {"Control", 1.0f, true,  false, "switch_camera_state", 26}
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

    if (GameState.Num() != GetStateSize())
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent: Invalid GameState size. Expected %d, got %d."), GetStateSize(), GameState.Num());
        return 0;
    }

    // --- Prepare Input Tensor ---
    // The input tensor shape is [1, GetStateSize()] (batch_size = 1)
    FTensorShape InputShape = FTensorShape::Make({1, static_cast<uint32>(GetStateSize())});
    
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


bool UInferenceComponent::IstAktionMoeglich(int32 ActionIndex) const
{
    const ARLAgent* AgentFuerGruppe = Cast<ARLAgent>(GetOwner());
    AExtendedControllerBase* PCFuerGruppe = AgentFuerGruppe
        ? Cast<AExtendedControllerBase>(AgentFuerGruppe->GetController()) : nullptr;

    // Anlauf 5 (19.09.2026): den Zweitakt des Lehrers erzwingen.
    //
    // Gemessen ueber 290 454 Regelentscheidungen der Singularianer: war die vorige Aktion eine
    // Gruppenwahl, folgt zu 91,3 % eine Faehigkeit und zu 0,0 % eine weitere Gruppenwahl. War sie
    // es nicht, folgt zu 0,0 % eine Faehigkeit. Der Lehrer wechselt also streng ab - waehlen,
    // feuern, waehlen, feuern.
    //
    // Das nachgeahmte Netz faellt aus diesem Takt heraus: es waehlt nur noch in 0,8 % der Faelle
    // eine Gruppe (Lehrer 15,1 %) und erreicht den Zustand "Gruppe gewaehlt" damit praktisch nie.
    // Seine 13,4 % Faehigkeitsdruecke landen folglich alle im Zweig, in dem der Lehrer
    // Faehigkeiten NIE benutzt. Aus diesem Zweig fuehrt kein Weg zurueck, und genau deshalb waren
    // mehr Epochen, breitere Netze und Entropiebonus wirkungslos: es ist kein Kapazitaets-,
    // sondern ein Rueckkopplungsproblem.
    //
    // Gesperrt werden ausschliesslich die beiden Faelle, die der Lehrer zu EXAKT 0,0 % spielt.
    // Die 91,3 % bleiben ungezwungen - die Maske nimmt das Unmoegliche weg, sie schreibt nichts
    // vor. Zu Partiebeginn ist LastChosenActionIndex -1, also gilt "keine Gruppe gewaehlt": das
    // Netz muss erst waehlen, bevor es feuern darf. Das ist der gewollte Einstieg in den Takt.
    if (GRLActionMask == 4)
    {
        const bool bVorigeWarGruppenwahl = (LastChosenActionIndex >= 0 && LastChosenActionIndex <= 9);
        const bool bIstGruppenwahl = (ActionIndex >= 0 && ActionIndex <= 9);
        const bool bIstFaehigkeit = (ActionIndex >= 10 && ActionIndex <= 15);

        if (bVorigeWarGruppenwahl && bIstGruppenwahl)
        {
            return false;
        }
        if (!bVorigeWarGruppenwahl && bIstFaehigkeit)
        {
            return false;
        }
        return true;
    }

    // Anlauf 4 (02.09.2026): leere Gruppenwahlen ausblenden.
    //
    // Gemessen ueber je 12 Partien: BC waehlt in 8,9 % der Faelle eine leere Gruppe, PPO mit
    // KL-Leine in 52,8 %. PPO driftet also in Aktionen, die nichts kosten und deshalb auch
    // keinen Gegendruck aus der Belohnung bekommen - genau die Vermutung aus dem Plan, jetzt
    // mit Zahlen. Die KL-Leine haelt das nicht auf (KL 0,0095 und trotzdem -40,3).
    // Die Aktionen 0-9 sind die Gruppenwahlen, Reihenfolge wie in InitializeActionSpace.
    if (GRLActionMask == 3)
    {
        if (ActionIndex < 0 || ActionIndex > 9 || !PCFuerGruppe)
        {
            return true;
        }
        static const int32 Tastencodes[10] = { 18, 9, 10, 1, 21, 22, 23, 24, 25, 26 };
        const int32 Code = Tastencodes[ActionIndex];
        FGameplayTag Gruppentag;
        switch (Code)
        {
        case 18: Gruppentag = PCFuerGruppe->KeyTagCtrlR; break;
        case  9: Gruppentag = PCFuerGruppe->KeyTagCtrlQ; break;
        case 10: Gruppentag = PCFuerGruppe->KeyTagCtrlE; break;
        case  1: Gruppentag = PCFuerGruppe->KeyTagCtrlW; break;
        case 21: Gruppentag = PCFuerGruppe->KeyTagCtrl1; break;
        case 22: Gruppentag = PCFuerGruppe->KeyTagCtrl2; break;
        case 23: Gruppentag = PCFuerGruppe->KeyTagCtrl3; break;
        case 24: Gruppentag = PCFuerGruppe->KeyTagCtrl4; break;
        case 25: Gruppentag = PCFuerGruppe->KeyTagCtrl5; break;
        default: Gruppentag = PCFuerGruppe->KeyTagCtrl6; break;
        }
        if (!Gruppentag.IsValid() || !GetWorld())
        {
            return true; // ohne Tag nichts entscheiden
        }
        // Dieselbe Bedingung wie AExtendedControllerBase::SelectUnitsWithTag - sonst zaehlt
        // die Maske etwas anderes als die Auswahl spaeter findet. Ein erster Versuch mit
        // TActorIterator und HasTag blockierte fast JEDE Gruppenwahl (1 statt ~700).
        if (!PCFuerGruppe->RTSGameMode || PCFuerGruppe->RTSGameMode->AllUnits.Num() == 0)
        {
            return true; // ohne Einheitenliste nichts entscheiden
        }
        const FGameplayTagContainer Behaelter(Gruppentag);
        for (AActor* A : PCFuerGruppe->RTSGameMode->AllUnits)
        {
            const AUnitBase* U = Cast<AUnitBase>(A);
            if (U && U->CanBeSelected && U->GetUnitState() != UnitData::Dead
                && U->TeamId == PCFuerGruppe->SelectableTeamId
                && U->UnitTags.HasAnyExact(Behaelter))
            {
                return true; // Gruppe ist besetzt
            }
        }
        return false; // Leerzug
    }

    // Nur die sechs Faehigkeitsdruecke (Aktionen 10-15) sind hier zu pruefen; alles andere
    // bleibt unangetastet.
    if (ActionIndex < 10 || ActionIndex > 15)
    {
        return true;
    }

    const ARLAgent* Agent = Cast<ARLAgent>(GetOwner());
    if (!Agent)
    {
        return true;
    }
    AExtendedControllerBase* PC = Cast<AExtendedControllerBase>(Agent->GetController());
    if (!PC || PC->SelectedUnits.Num() == 0)
    {
        // Ohne Auswahl trifft der Druck ohnehin nichts - aber das ist ein anderer Fall, den
        // die Nachwahl behandelt. Hier nicht zusaetzlich eingreifen.
        return true;
    }

    const EGASAbilityInputID InputID = static_cast<EGASAbilityInputID>(
        static_cast<int32>(EGASAbilityInputID::AbilityOne) + (ActionIndex - 10));

    // Erster Anlauf (20:42) hat NICHTS bewirkt: der Slot ist bei Kampfeinheiten gar nicht leer,
    // er haelt nur Ein-/Ausgraben statt eines Baus. Der Trichter blieb Zeile fuer Zeile gleich.
    // Deshalb greift die Maske jetzt nur dort, wo der gemessene Verlust entsteht: bei einer
    // Auswahl, die ein Arbeiter anfuehrt - denn nur dann laeuft der Abwurf des Bauplatzes an.
    // Als Bau-Merkmal dient die ConstructionCost der Faehigkeit; Ein-/Ausgraben und Upgrades
    // kosten dort nichts.
    const AUnitBase* Erster = PC->SelectedUnits.IsValidIndex(0) ? PC->SelectedUnits[0] : nullptr;
    if (!IsValid(Erster) || !Erster->IsWorker)
    {
        return true; // kein Arbeiterpfad - nicht eingreifen
    }

    for (AUnitBase* Selected : PC->SelectedUnits)
    {
        if (!IsValid(Selected) || !Selected->IsWorker)
        {
            continue;
        }
        TArray<TSubclassOf<UGameplayAbilityBase>> Array =
            PC->GetAbilityArrayForUnit(Selected, PC->AbilityArrayIndex);
        const TSubclassOf<UGameplayAbility> Gefunden = Selected->GetAbilityForInputID(InputID, Array);
        if (!Gefunden)
        {
            continue;
        }
        const UGameplayAbilityBase* CDO = Gefunden->GetDefaultObject<UGameplayAbilityBase>();
        if (!CDO)
        {
            return true; // im Zweifel zulassen
        }
        const FBuildingCost& K = CDO->ConstructionCost;
        const bool bKostetEtwas = (K.PrimaryCost > 0 || K.SecondaryCost > 0 || K.TertiaryCost > 0
            || K.RareCost > 0 || K.EpicCost > 0 || K.LegendaryCost > 0);

        if (GRLActionMask == 2)
        {
            // Anlauf 3 (02.09.2026), aus der Messung abgeleitet statt geraten.
            //
            // Gemessen ueber 12 Partien im Fenster 0-60 s: bei ALLEN 125 Abwuerfen war eine
            // GA_BuildBuilding_* aktiv - die Annahme von Anlauf 1 und 2, der Slot halte gar
            // keinen Bau, war falsch. Die Bauquote haengt monoton an den Kosten:
            //   LarvalPod 50 -> 92 %,  SynapseCluser 75 -> 92 %,  CarapacePod 50+25 -> 75 %,
            //   TresherRoot 100 -> 58 %, SomaticPool 150 -> 37 %, BroodHive 400 -> 0 (0/12).
            // Geist=0 ist also ein Bezahlbarkeitsproblem. Die Regel-KI erreichte im selben
            // Fenster 41 von 41.
            if (!bKostetEtwas)
            {
                return true; // kein Bau - nicht eingreifen
            }
            const AResourceGameMode* RGM =
                GetWorld() ? GetWorld()->GetAuthGameMode<AResourceGameMode>() : nullptr;
            if (!RGM)
            {
                return true; // ohne GameMode nichts entscheiden
            }
            if (RGM->CanAffordConstruction(K, PC->SelectableTeamId))
            {
                return true;
            }
            continue; // unbezahlbar - weiter suchen, vielleicht kann eine andere Einheit
        }

        if (bKostetEtwas)
        {
            return true; // baut tatsaechlich etwas
        }
    }
    return false;
}

int32 UInferenceComponent::SelectActionFromScores(const TArray<float>& Scores)
{
    if (Scores.Num() == 0)
    {
        return 0;
    }

    // Team-Override vor dem globalen Wert - siehe FindTemperatureOverride.
    float Temperature = GetSamplingTemperature();
    float ProTeam = 0.f;
    if (FindTemperatureOverride(ResolveOwningTeamId(), ProTeam))
    {
        Temperature = ProTeam;
    }

    if (Temperature <= 0.f)
    {
        // Greedy. Correct for a value network, wrong for a cloned policy: the teacher plays a mixture, and
        // always taking its most likely key collapses the agent onto that key. Measured on the Xeno model,
        // greedy emitted "select workers" 13650 times and "use ability 1" 39 times where the teacher used
        // them 15385 and 3387 times - an agent that selects endlessly and never builds.
        int32 Best = INDEX_NONE;
        for (int32 i = 0; i < Scores.Num(); ++i)
        {
            if (GRLActionMask != 0 && !IstAktionMoeglich(i))
            {
                continue;
            }
            if (Best == INDEX_NONE || Scores[i] > Scores[Best])
            {
                Best = i;
            }
        }
        if (Best == INDEX_NONE)
        {
            // Alles ausgeblendet waere schlimmer als gar keine Maske.
            Best = 0;
            for (int32 i = 1; i < Scores.Num(); ++i)
            {
                if (Scores[i] > Scores[Best]) { Best = i; }
            }
        }
        return Best;
    }

    // Sample from the softmax so the action mix matches what the network was taught. Subtract the max
    // before exponentiating - the raw scores are unbounded logits and would overflow.
    float MaxScore = Scores[0];
    for (int32 i = 1; i < Scores.Num(); ++i)
    {
        MaxScore = FMath::Max(MaxScore, Scores[i]);
    }

    TArray<float> Weights;
    Weights.SetNumUninitialized(Scores.Num());
    float Total = 0.f;
    for (int32 i = 0; i < Scores.Num(); ++i)
    {
        Weights[i] = FMath::Exp((Scores[i] - MaxScore) / Temperature);
        if (GRLActionMask != 0 && !IstAktionMoeglich(i))
        {
            Weights[i] = 0.f;
        }
        Total += Weights[i];
    }

    if (Total <= 0.f || !FMath::IsFinite(Total))
    {
        // Maske hat alles weggenommen: unmaskiert weiterwuerfeln statt Aktion 0 zu erzwingen.
        Total = 0.f;
        for (int32 i = 0; i < Scores.Num(); ++i)
        {
            Weights[i] = FMath::Exp((Scores[i] - MaxScore) / Temperature);
            Total += Weights[i];
        }
        if (Total <= 0.f || !FMath::IsFinite(Total))
        {
            return 0;
        }
    }

    float Roll = FMath::FRandRange(0.f, Total);
    for (int32 i = 0; i < Weights.Num(); ++i)
    {
        Roll -= Weights[i];
        if (Roll <= 0.f)
        {
            return i;
        }
    }
    return Weights.Num() - 1;
}

FString UInferenceComponent::GetActionFromRLModel(const FGameStateData& GameState)
{
    // Feed back what this agent did last step. The training data is recorded the same way, and without it
    // the model cannot tell "nothing selected yet" from "workers selected, now press the ability".
    FGameStateData StateWithHistory = GameState;
    StateWithHistory.LastActionIndex = LastChosenActionIndex;

    // Convert the input struct to the flat TArray<float> the model needs
    const TArray<float> GameStateArray = ConvertStateToArray(StateWithHistory);

    // --- Basic Checks ---
    if (!ModelInstance.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent: ChooseAction called but the model instance is not valid."));
        return TEXT("{}");
    }

    // This check is still useful to ensure the conversion function is correct
    if (GameStateArray.Num() != GetStateSize())
    {
        UE_LOG(LogTemp, Error, TEXT("InferenceComponent: Converted GameState size is incorrect. Expected %d, got %d."), GetStateSize(), GameStateArray.Num());
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
    
    const int32 BestActionIndex = SelectActionFromScores(QValues);

    LastChosenActionIndex = BestActionIndex;

    // DIAGNOSE (bleibt stehen bis abbestellt): siehe AktionsZaehler im Header.
    if (AktionsZaehler.Num() < ActionSpace.Num())
    {
        AktionsZaehler.SetNumZeroed(ActionSpace.Num());
    }
    if (AktionsZaehler.IsValidIndex(BestActionIndex))
    {
        ++AktionsZaehler[BestActionIndex];
        ++AktionenSeitBericht;
    }
    if (const UWorld* Welt = GetWorld())
    {
        const double Jetzt = Welt->GetTimeSeconds();
        if (AktionenSeitBericht > 0 && (Jetzt - LetzterAktionsBericht) >= 60.0)
        {
            LetzterAktionsBericht = Jetzt;
            BerichteAktionsverteilung();
        }
    }

    // --- Return the chosen action as a JSON string ---
    return GetActionAsJSON(BestActionIndex);
}

void UInferenceComponent::BerichteAktionsverteilung()
{
    // Nur die belegten Eintraege, absteigend - eine Zeile je Minute Spielzeit reicht, um zu sehen,
    // worauf das Netz seine Entscheidungen verteilt.
    TArray<TPair<int32, int32>> Sortiert;
    for (int32 i = 0; i < AktionsZaehler.Num(); ++i)
    {
        if (AktionsZaehler[i] > 0)
        {
            Sortiert.Add(TPair<int32, int32>(i, AktionsZaehler[i]));
        }
    }
    Sortiert.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
    {
        return A.Value > B.Value;
    });

    FString Zeile;
    for (int32 i = 0; i < Sortiert.Num() && i < 10; ++i)
    {
        const int32 Index = Sortiert[i].Key;
        FString Name = TEXT("?");
        if (ActionSpace.IsValidIndex(Index))
        {
            Name = FString::Printf(TEXT("%s%d"), *ActionSpace[Index].Action, ActionSpace[Index].CameraState);
        }
        Zeile += FString::Printf(TEXT("%d:%s=%d "), Index, *Name, Sortiert[i].Value);
    }

    UE_LOG(LogTemp, Warning, TEXT("[NetzAktion] Team=%d seit letztem Bericht=%d, haeufigste: %s"),
        ResolveOwningTeamId(), AktionenSeitBericht, *Zeile);

    AktionenSeitBericht = 0;
}

FString UInferenceComponent::ChooseJsonAction(const FGameStateData& GameState)
{
    const EBrainMode ActiveMode = GetEffectiveBrainMode();

    if (ActiveMode == EBrainMode::RL_Model)
    {
        // Cheap no-op once resolved; covers possession not being complete on the first decisions.
        EnsureModelInitialised();

        // --- 1. Use the RL Brain ---
        return GetActionFromRLModel(GameState);
    }
    
    if (ActiveMode == EBrainMode::Behavior_Tree)
    {
        if (!BlackboardComp || !BehaviorTreeComp)
        {
            UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::ChooseJsonAction(BT): BlackboardComp (%p) or BehaviorTreeComp (%p) is null. Did you call InitializeBehaviorTree() with a valid AIController? Is StrategyBehaviorTree assigned?"), BlackboardComp, BehaviorTreeComp);
            return TEXT("{}");
        }

        // --- 2. Use the BT Brain ---
        UE_LOG(LogTemp, Verbose, TEXT("InferenceComponent::ChooseJsonAction(BT): Updating Blackboard from GameState and ticking BT once."));
        // A. Update the Blackboard with the latest game state
        UpdateBlackboard(GameState);

        // B. Clear the old action from the Blackboard
        BlackboardComp->SetValueAsString(TEXT("SelectedActionJSON"), TEXT(""));

        // C. Tick the Behavior Tree (using 0.1f from ARLAgent's timer)
        BehaviorTreeComp->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

        // D. Get the action chosen by the BT task
        const FString ChosenAction = BlackboardComp->GetValueAsString(TEXT("SelectedActionJSON"));

        if (!ChosenAction.IsEmpty())
        {
            UE_LOG(LogTemp, Verbose, TEXT("InferenceComponent::ChooseJsonAction(BT): SelectedActionJSON set (len=%d)."), ChosenAction.Len());
            return ChosenAction;
        }

        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::ChooseJsonAction(BT): SelectedActionJSON is empty after BT tick. Ensure a task (e.g., UBTT_ChooseAction_RuleBased) writes to this key and the Blackboard has a String key named 'SelectedActionJSON'."));
        // Failsafe: if BT doesn't pick an action, do nothing
        return TEXT("{}"); 
    }

    UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::ChooseJsonAction: No valid brain mode selected or BT not initialized."));
    return TEXT("{}");
}

void UInferenceComponent::InitializeBehaviorTree(AController* OwnerController)
{
    if (BrainMode != EBrainMode::Behavior_Tree)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::InitializeBehaviorTree skipped: BrainMode is not Behavior_Tree (current=%d)."), static_cast<int32>(BrainMode));
        return;
    }
    if (!StrategyBehaviorTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::InitializeBehaviorTree: StrategyBehaviorTree is NOT assigned. Assign a BT asset on the component."));
        return;
    }
    if (!OwnerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::InitializeBehaviorTree: OwnerController is null. Ensure you call this after possession with a valid AI Controller."));
        return;
    }

    // Preferred: if the owner is an AIController, use the standard helpers. They will create
    // UBlackboardComponent and UBehaviorTreeComponent for you and wire everything correctly.
    if (AAIController* AI = Cast<AAIController>(OwnerController))
    {
        UBlackboardComponent* OutBB = nullptr;
        if (AI->UseBlackboard(StrategyBehaviorTree->BlackboardAsset, OutBB))
        {
            BlackboardComp = OutBB;

            const bool bStarted = AI->RunBehaviorTree(StrategyBehaviorTree);
            if (!bStarted)
            {
                UE_LOG(LogTemp, Error, TEXT("InferenceComponent: RunBehaviorTree failed for '%s'"), *StrategyBehaviorTree->GetName());
            }

            BehaviorTreeComp = Cast<UBehaviorTreeComponent>(AI->GetBrainComponent());
            UE_LOG(LogTemp, Log, TEXT("InferenceComponent: BT initialized via AAIController (UseBlackboard/RunBehaviorTree)."));
            return;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("InferenceComponent: UseBlackboard failed. Check BT's Blackboard asset on '%s'."), *StrategyBehaviorTree->GetName());
        }
    }

    // Fallback (non-AIController owner): create and register components manually.
    // Note: This path is less common; prefer owning the BT via an AAIController.
    if (bRequireAIControllerForBT)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent: bRequireAIControllerForBT=true and OwnerController is not an AAIController. Skipping manual BT initialization. Ensure the pawn is possessed by an AAIController (e.g., ARTSBTController) which calls UseBlackboard/RunBehaviorTree."));
        return;
    }

    BlackboardComp = NewObject<UBlackboardComponent>(OwnerController, TEXT("BlackboardComp"));
    if (BlackboardComp)
    {
        BlackboardComp->RegisterComponent();
        BlackboardComp->InitializeBlackboard(*StrategyBehaviorTree->BlackboardAsset);
    }

    BehaviorTreeComp = NewObject<UBehaviorTreeComponent>(OwnerController, TEXT("BehaviorTreeComp"));
    if (BehaviorTreeComp)
    {
        BehaviorTreeComp->RegisterComponent();
        BehaviorTreeComp->StartTree(*StrategyBehaviorTree);
    }

    if (BehaviorTreeComp && BlackboardComp)
    {
        UE_LOG(LogTemp, Log, TEXT("InferenceComponent: Behavior Tree initialized (manual components)."));
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
    BlackboardComp->SetValueAsFloat(TEXT("TertiaryResource"), GameState.TertiaryResource);
    BlackboardComp->SetValueAsFloat(TEXT("RareResource"), GameState.RareResource);
    BlackboardComp->SetValueAsFloat(TEXT("EpicResource"), GameState.EpicResource);
    BlackboardComp->SetValueAsFloat(TEXT("LegendaryResource"), GameState.LegendaryResource);

    BlackboardComp->SetValueAsVector(TEXT("AgentPosition"), GameState.AgentPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageFriendlyPosition"), GameState.AverageFriendlyPosition);
    BlackboardComp->SetValueAsVector(TEXT("AverageEnemyPosition"), GameState.AverageEnemyPosition);

    // Per-tag counts: Alt1..Alt6
    BlackboardComp->SetValueAsInt(TEXT("Alt1TagFriendlyUnitCount"), GameState.Alt1TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt1TagEnemyUnitCount"), GameState.Alt1TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt2TagFriendlyUnitCount"), GameState.Alt2TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt2TagEnemyUnitCount"), GameState.Alt2TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt3TagFriendlyUnitCount"), GameState.Alt3TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt3TagEnemyUnitCount"), GameState.Alt3TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt4TagFriendlyUnitCount"), GameState.Alt4TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt4TagEnemyUnitCount"), GameState.Alt4TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt5TagFriendlyUnitCount"), GameState.Alt5TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt5TagEnemyUnitCount"), GameState.Alt5TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt6TagFriendlyUnitCount"), GameState.Alt6TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Alt6TagEnemyUnitCount"), GameState.Alt6TagEnemyUnitCount);

    // Ctrl1..Ctrl6
    BlackboardComp->SetValueAsInt(TEXT("Ctrl1TagFriendlyUnitCount"), GameState.Ctrl1TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl1TagEnemyUnitCount"), GameState.Ctrl1TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl2TagFriendlyUnitCount"), GameState.Ctrl2TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl2TagEnemyUnitCount"), GameState.Ctrl2TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl3TagFriendlyUnitCount"), GameState.Ctrl3TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl3TagEnemyUnitCount"), GameState.Ctrl3TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl4TagFriendlyUnitCount"), GameState.Ctrl4TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl4TagEnemyUnitCount"), GameState.Ctrl4TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl5TagFriendlyUnitCount"), GameState.Ctrl5TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl5TagEnemyUnitCount"), GameState.Ctrl5TagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl6TagFriendlyUnitCount"), GameState.Ctrl6TagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("Ctrl6TagEnemyUnitCount"), GameState.Ctrl6TagEnemyUnitCount);

    // Ctrl Q/W/E/R
    BlackboardComp->SetValueAsInt(TEXT("CtrlQTagFriendlyUnitCount"), GameState.CtrlQTagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlQTagEnemyUnitCount"), GameState.CtrlQTagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlWTagFriendlyUnitCount"), GameState.CtrlWTagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlWTagEnemyUnitCount"), GameState.CtrlWTagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlETagFriendlyUnitCount"), GameState.CtrlETagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlETagEnemyUnitCount"), GameState.CtrlETagEnemyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlRTagFriendlyUnitCount"), GameState.CtrlRTagFriendlyUnitCount);
    BlackboardComp->SetValueAsInt(TEXT("CtrlRTagEnemyUnitCount"), GameState.CtrlRTagEnemyUnitCount);

    // NOTE: You should add GameTime to your FGameStateData struct and update it here
    // BlackboardComp->SetValueAsFloat(TEXT("GameTime"), GameState.GameTime);
}



void UInferenceComponent::PushBlackboardFromGameState(const FGameStateData& GameState)
{
    // External entry point to push Blackboard values without ticking the BT.
    // Useful for controller-driven BT where we only want to mirror state.
    if (BrainMode != EBrainMode::Behavior_Tree)
    {
        UE_LOG(LogTemp, Verbose, TEXT("InferenceComponent::PushBlackboardFromGameState called while BrainMode != Behavior_Tree (current=%d). Skipping."), static_cast<int32>(BrainMode));
        return;
    }

    if (!BlackboardComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("InferenceComponent::PushBlackboardFromGameState: BlackboardComp is null. Ensure InitializeBehaviorTree was called and a BT/Blackboard is running."));
        return;
    }

    UpdateBlackboard(GameState);
}
