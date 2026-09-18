// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToResourceExtractionStateProcessor.h"
#include "Components/StaticMeshComponent.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/WorkingUnitBase.h" // Header for this processor
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"     // For FTransformFragment
#include "MassMovementFragments.h"   // For FMassMoveTargetFragment, FMassVelocityFragment (optional but good practice)
#include "MassSignalSubsystem.h"
#include "Async/Async.h"

// --- Include your specific project Fragments, Tags, and Signals ---
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"       // Contains State Tags (FMassStateGoToResourceExtractionTag)
#include "Mass/UnitNavigationFragments.h"  // FUnitNavigationPathFragment - nur fuer die Stall-Diagnose
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"
#include "ProfilingDebugging/CsvProfiler.h"


UGoToResourceExtractionStateProcessor::UGoToResourceExtractionStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToResourceExtractionStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::All);

    // Fragments needed:
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);       // Read TargetEntity, LastKnownLocation, bHasValidTarget
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);         // Read current location for distance check
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);    // Read RunSpeed
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);   // Read ResourceArrivalDistance
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);    // Write new move target / stop movement
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // Client prediction: filled on the client to anticipate the server stop (Optional so the
    // server query still matches entities that lack it). Actor is read for MovementAcceptanceRadius.
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    // NUR fuer die Diagnose: der Pfadzustand ist die verbliebene offene Frage bei den
    // GoToResource-Haengern. Optional, damit die Query weiterhin jede Entitaet trifft.
    EntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    // EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // Might read velocity to see if stuck? Optional.


    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToResourceExtractionStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UGoToResourceExtractionStateProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UGoToResourceExtractionStateProcessor);

    //QUICK_SCOPE_CYCLE_COUNTER(STAT_UGoToResourceExtractionStateProcessor_Execute);
    //TRACE_CPUPROFILER_EVENT_SCOPE(UGoToResourceExtractionStateProcessor_Execute);
    
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun -= ExecutionInterval;

    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

// CLIENT: prediction only. The previous build re-authored worker state tags here (and self-
// signalled), which fought the bubble's tag re-application without hysteresis and caused the
// documented per-cycle GetUnitState flip / broken animation. That is gone: the bubble is the sole
// tag authority for workers. ResourceAvailable / BuildingAreaAvailable live in the un-replicated
// WorkerStats and are meaningless on the client, so we ignore them and only predict movement
// toward the replicated MoveTarget.Center.
void UGoToResourceExtractionStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        if (PredList.Num() == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FVector CurrentLocation = TransformList[i].GetTransform().GetLocation();
            const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            float ArrivalDistance = ArrivalDistanceMultiplier * 50.f; // fallback if actor not resolved yet
            if (ActorList.Num() > 0)
            {
                if (const AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].Get()))
                {
                    ArrivalDistance = ArrivalDistanceMultiplier * Unit->MovementAcceptanceRadius;
                }
            }

            PredictWorkerStop(PredList[i], CurrentLocation, MoveTarget.Center, MoveTarget.DesiredSpeed.Get(), ArrivalDistance);
        }
    });
}

void UGoToResourceExtractionStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld(); // Get World via Context
    if (!World) return;

    if (!SignalSubsystem) return;

    // Using deferred signal commands via Context, no manual arrays or AsyncTask dispatch needed

    EntityQuery.ForEachEntityChunk(Context,
        // Capture World for helper functions
        [this, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const auto CombatStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        // Optional - kann leer sein, deshalb vor jedem Zugriff auf Num() pruefen.
        const auto PathList = ChunkContext.GetFragmentView<FUnitNavigationPathFragment>();
        // Optional: nur fuer die Groesse der Lagerstaette (siehe Ankunftsrechnung unten).
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& AIState = AIStateList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassWorkerStatsFragment& WorkerStatsFrag = WorkerStatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FMassCombatStatsFragment& CombatStats = CombatStatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            AIState.StateTimer += ExecutionInterval;

            // --- 1. Check Target Validity ---

            if (!WorkerStatsFrag.ResourceAvailable)
            {
                // Target is lost or invalid. Signal to go idle or find a new task.
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBase, Entity);
                }
                continue;
            }

            if (WorkerStatsFrag.BuildingAreaAvailable && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;

                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBuild, Entity);
                }
                continue;
            }

            const float DistanceToTargetCenter = FVector::Dist2D(Transform.GetLocation(), WorkerStatsFrag.ResourcePosition);

            MoveTarget.DistanceToGoal = DistanceToTargetCenter - WorkerStatsFrag.ResourceArrivalDistance; // Update distance

            if (DistanceToTargetCenter <= WorkerStatsFrag.ResourceArrivalDistance && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;

                // Stop movement and mirror to clients when reaching the resource
                StopMovement(MoveTarget, World);
                // Queue signals thread-safely using the deferred command buffer
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::ResourceExtraction, Entity);
                }
                continue;
            }

            // REINE DIAGNOSE, kein Eingriff - erst messen, dann entscheiden.
            // Auffaellig ist zweierlei: dieser Prozessor hat KEINEN Fortschrittswaechter, und das
            // Auffrischen des Bewegungsziels ist auskommentiert (Zeile darunter). Das Ziel wird
            // also nur einmal beim Betreten des Zustands gesetzt. Bleibt der Arbeiter unterwegs
            // haengen, holt ihn nichts wieder heraus. `GoToResource` ist nach den Chase- und
            // Run-Korrekturen die groesste verbliebene Haenger-Kategorie (36-60 je Lauf).
            {
                const FVector Jetzt = Transform.GetLocation();
                if (FVector::Dist2D(Jetzt, AIState.ResStallLocation) > 40.f)
                {
                    AIState.ResStallLocation = Jetzt;
                    AIState.ResStallTimer = 0.f;
                    AIState.bResStallReported = false;
                }
                else
                {
                    AIState.ResStallTimer += ExecutionInterval;
                    if (AIState.ResStallTimer >= 8.f && !AIState.bResStallReported)
                    {
                        AIState.bResStallReported = true;

                        // Die frueheren Spalten ZielBefehlAbw / RessPos / Center sind ERLEDIGT und
                        // wurden entfernt: der Abstand zwischen MoveTarget.Center und der Ressource
                        // ist KEIN Fehlerindikator. Der Server klemmt Center selbst auf das Ende des
                        // begehbaren Pfades (UnitMovementProcessor.cpp:726) - ein Center abseits der
                        // Ressource ist der Normalzustand. Die Endlosschleife, die aus dem Vergleich
                        // dagegen entstand, ist behoben; die Stall-Zahl blieb dabei unveraendert
                        // (19 bzw. 38 gegen Basislinie 19). Die Ursache liegt also NICHT beim Ziel.
                        //
                        // Geprueft wird jetzt der Pfadzustand - dieselbe Signatur wie beim
                        // Chase-Befund: ein entarteter Pfad (2 Punkte = Luftlinie) fuehrt die
                        // Einheit gegen Geometrie, ohne dass eine neue Suche ausgeloest wird.
                        //   SollTempo == 0     -> es liegt gar kein Bewegungsbefehl an
                        //   Sucht == 1         -> haengt in der Pfadsuche (dabei wird das Tempo genullt)
                        //   Pfadpunkte == 2    -> Luftlinie, keine echte Route
                        //   Pfadpunkte == -1   -> die Entity hat das Fragment gar nicht
                        //   NaechsterWP gross  -> Wegpunkt unerreichbar, Einheit blockiert davor
                        int32 PfadPunkte = -1;
                        int32 PfadIndex = -1;
                        int32 SuchtGerade = -1;
                        float NaechsterWP = -1.f;
                        if (PathList.Num() > i)
                        {
                            const FUnitNavigationPathFragment& Nav = PathList[i];
                            SuchtGerade = Nav.bIsPathfindingInProgress ? 1 : 0;
                            PfadIndex = Nav.CurrentPathPointIndex;
                            if (Nav.CurrentPath.IsValid())
                            {
                                const TArray<FNavPathPoint>& Punkte = Nav.CurrentPath->GetPathPoints();
                                PfadPunkte = Punkte.Num();
                                if (Punkte.IsValidIndex(Nav.CurrentPathPointIndex))
                                {
                                    NaechsterWP = FVector::Dist2D(Jetzt, Punkte[Nav.CurrentPathPointIndex].Location);
                                }
                            }
                            else
                            {
                                PfadPunkte = 0;
                            }
                        }

                        UE_LOG(LogTemp, Warning,
                            TEXT("[ResourceStall] bei (%.0f, %.0f) DistRessource=%.0f Ankunftsradius=%.0f SollTempo=%.0f Schaltet=%d Pfadpunkte=%d Index=%d NaechsterWP=%.0f Sucht=%d"),
                            Jetzt.X, Jetzt.Y, DistanceToTargetCenter,
                            WorkerStatsFrag.ResourceArrivalDistance,
                            MoveTarget.DesiredSpeed.Get(),
                            AIState.SwitchingState ? 1 : 0,
                            PfadPunkte, PfadIndex, NaechsterWP, SuchtGerade);

                        // --- Eingriff: die Ankunft kann geometrisch unmoeglich geworden sein ---
                        //
                        // Gemessen ueber 2 Laeufe, 106 Meldungen: von den 59 Faellen unter 1000
                        // Einheiten Abstand standen 34 nachweislich am ENDE ihres Pfades und 36
                        // direkt an ihrem naechsten Wegpunkt; der Bewegungsbefehl lag dabei IMMER
                        // an (SollTempo nie 0) und keiner haing in der Pfadsuche. Medianabstand
                        // 294 bei einem Ankunftsradius von 125.
                        //
                        // Das ist ein Deadlock zwischen zwei verschiedenen Ankunftskriterien:
                        //   UnitMovementProcessor.cpp:488  haelt den Arbeiter fuer angekommen,
                        //     sobald er MoveTarget.Center erreicht - und Center ist auf das Ende
                        //     des BEGEHBAREN Pfades geklemmt (dort Zeile 726). Er setzt den Pfad
                        //     zurueck und plant nicht neu (if/else-if).
                        //   dieser Prozessor  wartet auf ResourcePosition +/- ResourceArrivalDistance,
                        //     also auf den ECHTEN Knoten.
                        // Reicht das Navmesh nicht bis auf 125 an den Knoten heran, widersprechen
                        // sich beide dauerhaft: der Laeufer ist fertig, der Zustandsautomat wartet
                        // ewig. Der Arbeiter steht bis zum Spielende.
                        //
                        // Deshalb: steht er am Pfadende und ist der Rest nur noch eine bekannte
                        // Restdistanz, gilt er als angekommen. Der Eingriff sitzt bewusst INNERHALB
                        // des Stillstandszweigs - er kann also nur Arbeiter treffen, die bereits
                        // 8 Sekunden ohne Fortschritt sind, und stoert den Normalbetrieb nicht.
                        const bool bAmPfadende = (PfadPunkte <= 0) || (PfadIndex >= PfadPunkte - 1);
                        const float Restschwelle = WorkerStatsFrag.ResourceArrivalDistance * StuckArrivalFactor;

                        if (bAmPfadende && !AIState.SwitchingState
                            && DistanceToTargetCenter <= Restschwelle)
                        {
                            AIState.SwitchingState = true;
                            StopMovement(MoveTarget, World);

                            UE_LOG(LogTemp, Warning,
                                TEXT("[ResourceReached] Ankunft am Pfadende anerkannt: Rest=%.0f Schwelle=%.0f Pfadpunkte=%d"),
                                DistanceToTargetCenter, Restschwelle, PfadPunkte);

                            if (SignalSubsystem)
                            {
                                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::ResourceExtraction, Entity);
                            }
                            continue;
                        }

                        // Die zweite Haelfte (47 von 106, Median 1436 Einheiten entfernt) bleibt
                        // BEWUSST ohne Eingriff. Dort ist das Bild uneinheitlich - nur 13 am
                        // Pfadende, 14 mit 4-Punkt- und 6 mit 9-Punkt-Pfaden - das sind ueberwiegend
                        // Arbeiter, die wirklich unterwegs sind und blockiert werden, nicht solche
                        // mit unmoeglicher Ankunft. Sie hier freizugeben hiesse, sie quer ueber die
                        // Karte "ankommen" zu lassen. Erst messen, was sie blockiert.
                        if (bAmPfadende && DistanceToTargetCenter > Restschwelle)
                        {
                            UE_LOG(LogTemp, Warning,
                                TEXT("[ResourceUnreachable] am Pfadende, aber noch %.0f entfernt (Schwelle %.0f) - kein Eingriff"),
                                DistanceToTargetCenter, Restschwelle);
                        }
                    }
                }
            }

            // Bewegungsziel auffrischen - aber NUR bei wesentlicher Abweichung.
            //
            // Der Aufruf stand hier auskommentiert (und steht es im GoToBaseStateProcessor immer
            // noch), weil `UpdateMoveTarget` intern `CreateNewAction` ausloest: jeden Takt
            // aufgerufen startet er die Bewegung endlos neu und die Einheit kommt nie los - genau
            // der Fehler, der bei Chase die Kampfeinheiten stillstehen liess.
            // Ganz weglassen ist aber auch falsch: das Ziel wird dann nur EINMAL beim Betreten
            // gesetzt, und wird der Arbeiter danach einer anderen Ressource zugewiesen, aendert
            // sich ResourcePosition, MoveTarget.Center aber nicht. Gemessen am 2026-08-14:
            // 8 von 19 Stall-Meldungen mit einem Bewegungsziel rund 1700 Einheiten NEBEN der
            // Ressource - der Arbeiter lief zum alten Punkt und kam nie an.
            // Die Toleranz loest beides: neu gesetzt wird nur, wenn das Ziel wirklich gewechselt hat.
            // Geprueft wird gegen den zuletzt BEFOHLENEN Punkt, nicht gegen MoveTarget.Center.
            //
            // Center taugt hier nicht als Gedaechtnis: der Server ueberschreibt es selbst mit dem
            // Ende des gefundenen Pfades (UnitMovementProcessor.cpp:726), sobald der Pfad mehr als
            // 10 Einheiten vor dem Ziel endet. Bei einem Ressourcenknoten ist das der Normalfall -
            // er hat Kollision, der Pfad endet davor. Der Vergleich gegen Center misst deshalb den
            // Abstand zum erreichbaren Punkt, findet ihn jeden Takt wieder zu gross und setzt das
            // Ziel endlos neu. Das war die Ursache der Arbeiter, die auf der Stelle laufen:
            // gemessen am 2026-08-14 feuerte [ResourceRetarget] fuer dieselben Ressourcenpositionen
            // in JEDEM Frame, weil UpdateMoveTarget intern CreateNewAction ruft und die Bewegung
            // damit endlos neu startet.
            //
            // Gegen den befohlenen Punkt geprueft feuert die Korrektur genau dann, wofuer sie
            // gedacht ist: wenn der Arbeiter einer ANDEREN Ressource zugewiesen wurde.
            const bool bZielGewechselt =
                FVector::Dist2D(AIState.LastCommandedResourcePosition, WorkerStatsFrag.ResourcePosition) > WorkerRetargetTolerance;

            if (!AIState.SwitchingState && bZielGewechselt)
            {
                const FVector Vorher = AIState.LastCommandedResourcePosition;

                UpdateMoveTarget(MoveTarget, WorkerStatsFrag.ResourcePosition, CombatStats.RunSpeed, World);
                AIState.LastCommandedResourcePosition = WorkerStatsFrag.ResourcePosition;

                // Zweite Zeile NACH dem Eingriff - die erste steht davor und konnte deshalb gar
                // nicht zeigen, ob die Korrektur ueberhaupt ausgefuehrt wird. Genau daran ist die
                // Bewertung in Runde 88 gescheitert.
                UE_LOG(LogTemp, Warning,
                    TEXT("[ResourceRetarget] Ziel neu gesetzt auf (%.0f, %.0f), vorher (%.0f, %.0f), Abweichung war %.0f"),
                    WorkerStatsFrag.ResourcePosition.X, WorkerStatsFrag.ResourcePosition.Y,
                    Vorher.X, Vorher.Y,
                    FVector::Dist2D(Vorher, WorkerStatsFrag.ResourcePosition));
            }
        }
    }); // End ForEachEntityChunk


}
