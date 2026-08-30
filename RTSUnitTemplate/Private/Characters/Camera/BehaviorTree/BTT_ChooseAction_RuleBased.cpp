// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Camera/BehaviorTree/BTT_ChooseAction_RuleBased.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"

namespace
{
    /**
     * DAgger-Mischung. Anteil 0..1 der Entscheidungen, die in einer Modellpartie die REGEL-KI trifft
     * und aufzeichnet.
     *
     * Warum ueberhaupt: geklontes Verhalten trifft die Aktionsverteilung des Lehrers im Mittel gut
     * (gemessen: groesste Aktion 19,6 % gegen 21,9 %), ordnet sie aber dem falschen Zustand zu
     * (Uebereinstimmung 39,6 %). Dadurch geraet das Netz in Lagen, die der Lehrer nie besucht, und dort
     * hat es nie ein Beispiel gesehen. Genau dagegen gibt es DAgger: Zustaende aus dem Spiel des
     * SCHUELERS, Antworten vom LEHRER.
     *
     * Den Lehrer nur "nebenbei zu fragen" geht hier nicht - ChooseJsonActionRuleBased wertet die
     * Verteidigung aus, setzt Abklingzeiten und fuehrt Regelzeilen aus. Deshalb die beta-Mischung aus
     * der Originalarbeit: der Lehrer entscheidet manchmal wirklich, mit allen seinen Nebenwirkungen.
     *
     * 0 = aus (reines Modellspiel, wie bisher). Nur fuer die Datensammlung gedacht, nicht fuer Messungen:
     * eine Partie mit beta > 0 misst eine Mischung aus beiden, nicht die Staerke des Netzes.
     */
    /**
     * Wie viele Aktionen das Netz je Entscheidung ausfuehren darf.
     *
     * Der Grund ist gemessen, nicht vermutet: die Regel-KI baut pro Entscheidung eine
     * ZUSAMMENGESETZTE Aktion aus mehreren Indizes ("Kontrollgruppe waehlen, dann Taste
     * druecken") und fuehrt sie komplett aus. Das Netz liefert genau einen Index je Abfrage.
     * Der Verhaltensbaum fragt beide gleich oft - herausgekommen sind 2730 Aktionen je Partie
     * beim Lehrer gegen 529 beim Netz, also Faktor 5,2. Der Leistungsabstand hat dieselbe
     * Groessenordnung: das Netz handelt nicht schlechter, es handelt seltener.
     *
     * 1 = wie bisher, aendert nichts.
     */
    static int32 GRLAktionenJeEntscheidung = 1;
    static FAutoConsoleVariableRef CVarRLAktionenJeEntscheidung(
        TEXT("rts.rl.actions.per.decision"),
        GRLAktionenJeEntscheidung,
        TEXT("Wie viele Aktionen das Netz je Entscheidung ausfuehrt (1 = wie bisher)."),
        ECVF_Default);

    static float GRLDaggerBeta = 0.f;
    static FAutoConsoleVariableRef CVarRLDaggerBeta(
        TEXT("rts.rl.dagger.beta"),
        GRLDaggerBeta,
        TEXT("DAgger-Mischung: Anteil der Zuege, die in einer Modellpartie die Regel-KI uebernimmt (0..1)."),
        ECVF_Default);
}
#include "Characters/Camera/RL/InferenceComponent.h"
#include "Characters/Camera/RL/RLRecorderSubsystem.h" // for FGameStateData

UBTT_ChooseAction_RuleBased::UBTT_ChooseAction_RuleBased()
{
	NodeName = TEXT("Choose Action (Rule-Based)");
	BlackboardKey.AddStringFilter(this, GET_MEMBER_NAME_CHECKED(UBTT_ChooseAction_RuleBased, BlackboardKey));
}

EBTNodeResult::Type UBTT_ChooseAction_RuleBased::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Missing BlackboardComponent. Ensure your Behavior Tree has a valid Blackboard asset and UseBlackboard was called by an AAIController."));
        return EBTNodeResult::Failed;
    }

    // Prefer AAIController if available, but don't require it. Fallback to AvatarActor (Pawn).
    AAIController* Controller = OwnerComp.GetAIOwner();
    APawn* Pawn = nullptr;
    if (Controller)
    {
        Pawn = Controller->GetPawn();
        if (!Pawn)
        {
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: AAIController present but has no Pawn. Was the pawn possessed?"));
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from AAIController: %s"), *Pawn->GetName());
        }
    }

    if (!Pawn)
    {
        // Fallback: inspect the owner of the behavior tree component
        AActor* OwnerActor = OwnerComp.GetOwner();
        if (OwnerActor)
        {
            // If Owner is any Controller (AI or Player), ask it for its Pawn
            if (AController* AnyController = Cast<AController>(OwnerActor))
            {
                Pawn = AnyController->GetPawn();
                if (Pawn)
                {
                    if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from Controller owner (%s): %s"), *OwnerActor->GetClass()->GetName(), *Pawn->GetName());
                }
                else
                {
                    if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Controller owner (%s) has no Pawn (not possessed?)."), *OwnerActor->GetClass()->GetName());
                }
            }
            else
            {
                // In some manual setups the owner itself might be a Pawn
                Pawn = Cast<APawn>(OwnerActor);
                if (Pawn)
                {
                    if (bDebug) UE_LOG(LogTemp, Verbose, TEXT("BTT_ChooseAction_RuleBased: Using Owner actor cast to Pawn: %s"), *Pawn->GetName());
                }
            }
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: BehaviorTreeComponent has no Owner actor. Cannot resolve Pawn."));
        }
    }

    if (!Pawn && BB && !AgentPawnKey.IsNone())
    {
        if (UObject* Obj = BB->GetValueAsObject(AgentPawnKey))
        {
            Pawn = Cast<APawn>(Obj);
            if (Pawn)
            {
                if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Using Pawn from BB '%s': %s"), *AgentPawnKey.ToString(), *Pawn->GetName());
            }
        }
    }

    if (!Pawn)
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Failed to resolve Pawn. Aborting task."));
        return EBTNodeResult::Failed;
    }

    URTSRuleBasedDeciderComponent* Decider = Pawn->FindComponentByClass<URTSRuleBasedDeciderComponent>();
    if (!Decider)
    {
        if (bDebug) UE_LOG(LogTemp, Error, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no URTSRuleBasedDeciderComponent."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    FGameStateData GS;

    auto SafeGetBBFloat = [&](FName KeyName) -> float
    {
        if (KeyName.IsNone()) return 0.0f;
        float Val = BB->GetValueAsFloat(KeyName);
        if (Val == 0.0f)
        {
            // Try reading as Int in case the user defined it as Int in Blackboard
            Val = (float)BB->GetValueAsInt(KeyName);
        }
        return Val;
    };

    auto SafeGetBBInt = [&](FName KeyName) -> int32
    {
        if (KeyName.IsNone()) return 0;
        return BB->GetValueAsInt(KeyName);
    };

    GS.MyUnitCount = SafeGetBBInt(MyUnitCountKey);
    GS.EnemyUnitCount = SafeGetBBInt(EnemyUnitCountKey);
    GS.PrimaryResource = SafeGetBBFloat(PrimaryResourceKey);
    GS.SecondaryResource = SafeGetBBFloat(SecondaryResourceKey);
    GS.TertiaryResource = SafeGetBBFloat(TertiaryResourceKey);
    GS.RareResource = SafeGetBBFloat(RareResourceKey);
    GS.EpicResource = SafeGetBBFloat(EpicResourceKey);
    GS.LegendaryResource = SafeGetBBFloat(LegendaryResourceKey);

    GS.MaxPrimaryResource = SafeGetBBFloat(MaxPrimaryResourceKey);
    GS.MaxSecondaryResource = SafeGetBBFloat(MaxSecondaryResourceKey);
    GS.MaxTertiaryResource = SafeGetBBFloat(MaxTertiaryResourceKey);
    GS.MaxRareResource = SafeGetBBFloat(MaxRareResourceKey);
    GS.MaxEpicResource = SafeGetBBFloat(MaxEpicResourceKey);
    GS.MaxLegendaryResource = SafeGetBBFloat(MaxLegendaryResourceKey);

    if (bDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Reading Blackboard Keys:"));
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxPrimaryResourceKey.ToString(), GS.MaxPrimaryResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxSecondaryResourceKey.ToString(), GS.MaxSecondaryResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxTertiaryResourceKey.ToString(), GS.MaxTertiaryResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxRareResourceKey.ToString(), GS.MaxRareResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxEpicResourceKey.ToString(), GS.MaxEpicResource);
        UE_LOG(LogTemp, Log, TEXT("  %s = %.2f"), *MaxLegendaryResourceKey.ToString(), GS.MaxLegendaryResource);
    }
    GS.AgentPosition = BB->GetValueAsVector(AgentPositionKey);
    GS.AverageEnemyPosition = BB->GetValueAsVector(AverageEnemyPositionKey);
    GS.AverageFriendlyPosition = BB->GetValueAsVector(TEXT("AverageFriendlyPosition"));

    // Additional totals
    GS.MyTotalHealth = SafeGetBBFloat(TEXT("MyTotalHealth"));
    GS.EnemyTotalHealth = SafeGetBBFloat(TEXT("EnemyTotalHealth"));

    // Populate friendly tag counts
    GS.Alt1TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt1TagFriendlyUnitCount"));
    GS.Alt2TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt2TagFriendlyUnitCount"));
    GS.Alt3TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt3TagFriendlyUnitCount"));
    GS.Alt4TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt4TagFriendlyUnitCount"));
    GS.Alt5TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt5TagFriendlyUnitCount"));
    GS.Alt6TagFriendlyUnitCount = SafeGetBBInt(TEXT("Alt6TagFriendlyUnitCount"));

    GS.Ctrl1TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl1TagFriendlyUnitCount"));
    GS.Ctrl2TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl2TagFriendlyUnitCount"));
    GS.Ctrl3TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl3TagFriendlyUnitCount"));
    GS.Ctrl4TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl4TagFriendlyUnitCount"));
    GS.Ctrl5TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl5TagFriendlyUnitCount"));
    GS.Ctrl6TagFriendlyUnitCount = SafeGetBBInt(TEXT("Ctrl6TagFriendlyUnitCount"));

    GS.CtrlQTagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlQTagFriendlyUnitCount"));
    GS.CtrlWTagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlWTagFriendlyUnitCount"));
    GS.CtrlETagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlETagFriendlyUnitCount"));
    GS.CtrlRTagFriendlyUnitCount = SafeGetBBInt(TEXT("CtrlRTagFriendlyUnitCount"));

    // Populate enemy tag counts
    GS.Alt1TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt1TagEnemyUnitCount"));
    GS.Alt2TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt2TagEnemyUnitCount"));
    GS.Alt3TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt3TagEnemyUnitCount"));
    GS.Alt4TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt4TagEnemyUnitCount"));
    GS.Alt5TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt5TagEnemyUnitCount"));
    GS.Alt6TagEnemyUnitCount = SafeGetBBInt(TEXT("Alt6TagEnemyUnitCount"));

    GS.Ctrl1TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl1TagEnemyUnitCount"));
    GS.Ctrl2TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl2TagEnemyUnitCount"));
    GS.Ctrl3TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl3TagEnemyUnitCount"));
    GS.Ctrl4TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl4TagEnemyUnitCount"));
    GS.Ctrl5TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl5TagEnemyUnitCount"));
    GS.Ctrl6TagEnemyUnitCount = SafeGetBBInt(TEXT("Ctrl6TagEnemyUnitCount"));

    GS.CtrlQTagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlQTagEnemyUnitCount"));
    GS.CtrlWTagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlWTagEnemyUnitCount"));
    GS.CtrlETagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlETagEnemyUnitCount"));
    GS.CtrlRTagEnemyUnitCount = SafeGetBBInt(TEXT("CtrlRTagEnemyUnitCount"));

    // Detailed Blackboard snapshot logs (multi-line)
    if (bDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: GS Snapshot -> MyUnits=%d, EnemyUnits=%d, MyHP=%.1f, EnemyHP=%.1f"),
            GS.MyUnitCount, GS.EnemyUnitCount, GS.MyTotalHealth, GS.EnemyTotalHealth);
        UE_LOG(LogTemp, Log, TEXT("  Resources -> Prim=%.2f/%.2f Sec=%.2f/%.2f Ter=%.2f/%.2f Rare=%.2f/%.2f Epic=%.2f/%.2f Leg=%.2f/%.2f"),
            GS.PrimaryResource, GS.MaxPrimaryResource, GS.SecondaryResource, GS.MaxSecondaryResource, GS.TertiaryResource, GS.MaxTertiaryResource,
            GS.RareResource, GS.MaxRareResource, GS.EpicResource, GS.MaxEpicResource, GS.LegendaryResource, GS.MaxLegendaryResource);
        UE_LOG(LogTemp, Log, TEXT("  Positions -> Agent=(%.1f,%.1f,%.1f) AvgFriendly=(%.1f,%.1f,%.1f) AvgEnemy=(%.1f,%.1f,%.1f)"),
            GS.AgentPosition.X, GS.AgentPosition.Y, GS.AgentPosition.Z,
            GS.AverageFriendlyPosition.X, GS.AverageFriendlyPosition.Y, GS.AverageFriendlyPosition.Z,
            GS.AverageEnemyPosition.X, GS.AverageEnemyPosition.Y, GS.AverageEnemyPosition.Z);
        UE_LOG(LogTemp, Log, TEXT("  Tags Friendly -> Alt=[%d,%d,%d,%d,%d,%d] Ctrl=[%d,%d,%d,%d,%d,%d] Keys[QWER]=[%d,%d,%d,%d]"),
            GS.Alt1TagFriendlyUnitCount, GS.Alt2TagFriendlyUnitCount, GS.Alt3TagFriendlyUnitCount, GS.Alt4TagFriendlyUnitCount, GS.Alt5TagFriendlyUnitCount, GS.Alt6TagFriendlyUnitCount,
            GS.Ctrl1TagFriendlyUnitCount, GS.Ctrl2TagFriendlyUnitCount, GS.Ctrl3TagFriendlyUnitCount, GS.Ctrl4TagFriendlyUnitCount, GS.Ctrl5TagFriendlyUnitCount, GS.Ctrl6TagFriendlyUnitCount,
            GS.CtrlQTagFriendlyUnitCount, GS.CtrlWTagFriendlyUnitCount, GS.CtrlETagFriendlyUnitCount, GS.CtrlRTagFriendlyUnitCount);
        UE_LOG(LogTemp, Log, TEXT("  Tags Enemy    -> Alt=[%d,%d,%d,%d,%d,%d] Ctrl=[%d,%d,%d,%d,%d,%d] Keys[QWER]=[%d,%d,%d,%d]"),
            GS.Alt1TagEnemyUnitCount, GS.Alt2TagEnemyUnitCount, GS.Alt3TagEnemyUnitCount, GS.Alt4TagEnemyUnitCount, GS.Alt5TagEnemyUnitCount, GS.Alt6TagEnemyUnitCount,
            GS.Ctrl1TagEnemyUnitCount, GS.Ctrl2TagEnemyUnitCount, GS.Ctrl3TagEnemyUnitCount, GS.Ctrl4TagEnemyUnitCount, GS.Ctrl5TagEnemyUnitCount, GS.Ctrl6TagEnemyUnitCount,
            GS.CtrlQTagEnemyUnitCount, GS.CtrlWTagEnemyUnitCount, GS.CtrlETagEnemyUnitCount, GS.CtrlRTagEnemyUnitCount);
    }

    // Which brain decides is a per-team setting on the InferenceComponent. This task is the only place the
    // decision is actually made - the RLAgent's own ChooseJsonAction path only runs with the shared-memory
    // bridge enabled - so a trained network would never be consulted if this went straight to the rules.
    FString Json;
    UInferenceComponent* BrainComponent = Pawn->FindComponentByClass<UInferenceComponent>();
    const bool bNetzIstZustaendig = BrainComponent
        && BrainComponent->GetEffectiveBrainMode() == EBrainMode::RL_Model;
    // DAgger: in einem beta-Anteil der Zuege uebernimmt der Lehrer, damit seine Antworten auf den
    // Zustaenden landen, in die das NETZ die Partie gebracht hat.
    const bool bLehrerUebernimmt = bNetzIstZustaendig
        && GRLDaggerBeta > 0.f
        && FMath::FRand() < GRLDaggerBeta;
    if (bNetzIstZustaendig && !bLehrerUebernimmt)
    {
        // Den Zustand aufzeichnen, den das Netz WIRKLICH gesehen hat. ChooseJsonAction setzt
        // intern LastActionIndex auf die vorige Aktion; das uebergebene GS bleibt davon
        // unberuehrt. Ohne diese Zeile stand in jeder Modellaufnahme -1 (gemessen: 1268 von
        // 1268 Zeilen), waehrend die Regelaufnahmen 20 verschiedene Werte hatten - PPO haette
        // damit auf Zustaenden gerechnet, die es so nie gab.
        // Mehrere Zuege je Entscheidung (siehe rts.rl.actions.per.decision). Der Zustand wird
        // dazwischen NICHT neu erhoben - der Lehrer arbeitet seine zusammengesetzte Aktion
        // ebenfalls auf einem einzigen Zustand ab.
        const int32 ZuegeJeEntscheidung = FMath::Max(1, GRLAktionenJeEntscheidung);
        URLRecorderSubsystem* Recorder = URLRecorderSubsystem::Get(Pawn);

        for (int32 Zug = 0; Zug < ZuegeJeEntscheidung; ++Zug)
        {
        const int32 VorigeAktion = BrainComponent->GetLastChosenActionIndex();

        const FString ZugJson = BrainComponent->ChooseJsonAction(GS);

        // Selbstspiel-Aufnahme (16.08.2026): der Aufnahmehaken sass bisher NUR im Regel-Decider
        // (BuildCompositeActionJSON). Sobald beide Teams das Netz benutzen, entstand deshalb eine
        // Partie ohne eine einzige Trainingszeile - gemessen: "samples":0. Hier fehlt das
        // Gegenstueck fuer den Modellpfad.
        if (Recorder)
        {
            const int32 ActionIndex = BrainComponent->GetLastChosenActionIndex();
            if (ActionIndex >= 0)
            {
                FGameStateData GSAufnahme = GS;
                GSAufnahme.LastActionIndex = VorigeAktion;
                Recorder->RecordSampleFromGameState(Decider->ResolveOwningTeamId(), GSAufnahme, ActionIndex,
                                                   ERLSampleSource::Model);
            }
        }

        if (Zug + 1 < ZuegeJeEntscheidung)
        {
            // Zwischenzuege sofort ausfuehren; der letzte laeuft unten ueber den normalen Weg,
            // damit Blackboard-Schreiben und Rueckgabewert unveraendert bleiben.
            if (!ZugJson.IsEmpty())
            {
                BrainComponent->ExecuteActionFromJSON(ZugJson);
            }
        }
        else
        {
            Json = ZugJson;
        }
        }
    }
    else
    {
        Json = Decider->ChooseJsonActionRuleBased(GS);

        // Hat der Lehrer fuer das Netz uebernommen, muss das Netz erfahren, was gespielt wurde.
        // Sonst steht im naechsten Zug ein veraltetes LastActionIndex im Zustandsvektor - und genau
        // dieses Merkmal traegt die Kopplung "erst auswaehlen, dann Faehigkeit druecken".
        if (bLehrerUebernimmt)
        {
            const int32 GespielteAktion = Decider->GetLastRecordedActionIndex();
            if (GespielteAktion >= 0)
            {
                BrainComponent->SetLastChosenActionIndex(GespielteAktion);
            }
        }
    }

    if (Json.IsEmpty())
    {
        if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Decider returned empty JSON. Check rule thresholds and component configuration on %s."), *Pawn->GetName());
        return EBTNodeResult::Failed;
    }

    // Immediate execution path
    if (bExecuteActionImmediately)
    {
        if (UInferenceComponent* Inference = Pawn->FindComponentByClass<UInferenceComponent>())
        {
            if (bDebug)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Executing action immediately via InferenceComponent (len=%d)."), Json.Len());
            }
            Inference->ExecuteActionFromJSON(Json);
        }
        else
        {
            if (bDebug) UE_LOG(LogTemp, Warning, TEXT("BTT_ChooseAction_RuleBased: Pawn %s has no UInferenceComponent; cannot execute action immediately."), *Pawn->GetName());
        }

        if (bAlsoWriteToBB)
        {
            BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
            if (bDebug)
            {
                UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Also wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
            }
        }
        return EBTNodeResult::Succeeded;
    }

    // Default: write to BB and let a controller consume it
    BB->SetValueAsString(GetSelectedBlackboardKey(), Json);
    if (bDebug) UE_LOG(LogTemp, Log, TEXT("BTT_ChooseAction_RuleBased: Wrote SelectedActionJSON (len=%d) to key '%s'."), Json.Len(), *GetSelectedBlackboardKey().ToString());
    return EBTNodeResult::Succeeded;
}
