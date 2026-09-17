// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/ChaseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationTypes.h"

#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Core/RTSUnitUtils.h"

#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"  // FUnitNavigationPathFragment - nur fuer die Stall-Diagnose
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "MassReplicationFragments.h"
#include "Async/Async.h"
#include "Controller/PlayerController/CustomControllerBase.h"


UChaseStateProcessor::UChaseStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UChaseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::All); // Nur Chase-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional); // Bewegungsziel setzen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional); // Geschwindigkeit setzen (zum Stoppen)
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    // Nur fuer die Stall-Diagnose: existiert ueberhaupt ein Navigationspfad?
    EntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UChaseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


FVector CalculateChaseOffset(const FMassEntityHandle& Entity, float MinRadius = 100.0f, float MaxRadius = 250.0f)
{
    if (!Entity.IsSet() || MinRadius >= MaxRadius || MinRadius < 0.f)
    {
        return FVector::ZeroVector;
    }

    // Use the entity index to seed our "randomness" deterministically.
    const uint32 Index = Entity.Index;

    // Use parts of the Golden Angle (137.5 degrees) for good angular distribution.
    const float AngleDeg = FMath::Fmod(static_cast<float>(Index) * 137.50776405f, 360.0f);

    // Vary the radius based on the index, ensuring it's within bounds.
    // We use a different multiplier to avoid radius aligning perfectly with angle for sequential indices.
    const float RadiusRange = MaxRadius - MinRadius;
    const float Radius = MinRadius + FMath::Fmod(static_cast<float>(Index) * 61.803398875f, RadiusRange);

    // Convert to radians for Cos/Sin
    const float AngleRad = FMath::DegreesToRadians(AngleDeg);

    // Calculate offset in X/Y plane
    return FVector(Radius * FMath::Cos(AngleRad),
                   Radius * FMath::Sin(AngleRad),
                   0.0f);
}

void UChaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
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

void UChaseStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const auto NetIDList = ChunkContext.GetFragmentView<FMassNetworkIDFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        const bool bHasPrediction = PredictionList.Num() > 0;

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // === BatchDiag (TEMP): unit still in Chase tag after a move command => command didn't strip Chase ===
            RTS_BatchDiagLog(TEXT("CHASE-CLIENT"), World, EntityManager, Entity,
                Cast<AUnitBase>(ActorList[i].Get()) ? Cast<AUnitBase>(ActorList[i].Get())->UnitIndex : -1,
                bHasPrediction ? &PredictionList[i] : nullptr);

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
                continue;
            }

            // If already dead, skip any client-side tag manipulation
            if (DoesEntityHaveTag(EntityManager, Entity, FMassStateDeadTag::StaticStruct()))
            {
                continue;
            }

            // If health zero or below, mark dead and skip further changes
            if (Stats.Health <= 0.f)
            {
                auto& Defer = ChunkContext.Defer();
                Defer.AddTag<FMassStateDeadTag>(Entity);
                continue;
            }

            const FTransform& Transform = TransformList[i].GetTransform();
            
            // Case 1: Hold position => switch to Idle locally
            if (StateFrag.HoldPosition)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    StateFrag.StoredLocation = Transform.GetLocation();
                    StateFrag.PlaceholderSignal = UnitSignals::Idle;

                    SwitchToPlaceholderState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }

            bool bIsTargetActive = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity);
            const bool bIsFriendlyActive = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.FriendlyTargetEntity);

            if (bIsFriendlyActive)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    SwitchToPlaceholderState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }

            if (!bIsTargetActive || (!TargetFrag.bHasValidTarget))
            {
                       
                if (!StateFrag.SwitchingStateClient)
                {
                    SwitchToPlaceholderState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }

            // Case 3: In range check for local state switch to Pause
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];

            if (bIsTargetActive)
            {
                const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

                const bool bTgtUsable = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity);
                const FMassAgentCharacteristicsFragment* TargetCharFrag = bTgtUsable ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
                const FTransformFragment* TargetTransformFrag = bTgtUsable ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
                const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

                const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
                
                const float Tolerance = 10.f; // Client-Side Prediction Bias
                const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange + Tolerance);

                if (DistSq <= AttackRangeSq)
                {
                    if (!StateFrag.SwitchingStateClient)
                    {
                        StateFrag.SwitchingStateClient = true;
                        StateFrag.StateTimerClient = 0.f;

                        // Tags lokal manipulieren, damit der PauseStateProcessor übernimmt
                        auto& Defer = ChunkContext.Defer();
                        Defer.RemoveTag<FMassStateChaseTag>(Entity);
                        Defer.AddTag<FMassStatePauseTag>(Entity);

                        if (World)
                        {
                            if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
                            {
                                if (AUnitClientBubbleInfo* Bubble = Cache->GetBubble(false))
                                {
                                    if (FUnitReplicationItem* Item = Bubble->Agents.FindItemByNetID(NetIDList[i].NetID))
                                    {
                                        Item->PredictionTimer = 0.f;
                                        Item->bPredictedLatch = false;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    // Update prediction to chase the target (Prediction Bias)
                    if (bHasPrediction)
                    {
                        FMassClientPredictionFragment& Pred = PredictionList[i];
                        Pred.Location = TargetFrag.LastKnownLocation;
                        Pred.PredDesiredSpeed = Stats.RunSpeed;
                        Pred.bHasData = true;
                        Pred.PredSource = 23; // [PredDiag]
                    }
                }
            }
            else
            {
            }
        }
    });
}

void UChaseStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    if (!SignalSubsystem) return;

    // Using Mass deferred command buffer for thread-safe signaling; no manual PendingSignals array needed.

    EntityQuery.ForEachEntityChunk(Context,
        // Use Mass deferred command buffer via ChunkContext.Defer() for thread-safe signaling.
        // Do NOT access SignalSubsystem directly from worker threads.
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop
        const bool bHasMoveTarget = MoveTargetList.Num() > 0;
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const auto NavPathList = ChunkContext.GetFragmentView<FUnitNavigationPathFragment>(); // nur Diagnose
        const bool bHasNavPathFrag = NavPathList.Num() > 0;

            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Target Lost ---
            StateFrag.StateTimer += ExecutionInterval;

            if (!Stats.bUseProjectile) // && TargetFrag.bHasValidTarget
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UseRangedAbilitys, Entity);
                }
            }

            // Haengengebliebenes SwitchingState loesen - und zwar VOR jedem Ausstieg aus dieser
            // Schleife. Der Aufruf stand frueher weiter unten, hinter dem "Ziel verloren"-Zweig,
            // der das Flag selbst setzt und dann per continue aussteigt: eine Einheit, die ihr
            // Ziel verlor, setzte das Flag, uebersprang den Watchdog, betrat im naechsten Tick
            // denselben Zweig und blieb so fuer den Rest ihres Lebens bewegungslos in Chase
            // stehen - gemessen 160 s ohne einen einzigen Positionswechsel. Der Watchdog konnte
            // ausgerechnet die Einheiten nicht retten, fuer die er gebaut wurde.
            RTSUnitUtils::TickSwitchingStateWatchdog(StateFrag, ExecutionInterval);

            if (StateFrag.HoldPosition)
            {
                StateFrag.StoredLocation = Transform.GetLocation();
                StateFrag.SwitchingState = true;

                if (bHasMoveTarget)
                {
                    StopMovement(MoveTargetList[i], World);
                }

                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                }
                continue;
            }
            
            bool bIsTargetActive = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity);
            const bool bIsFriendlyActive = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.FriendlyTargetEntity);

            // Fortschrittswaechter - dieselbe Luecke, die der RunStateProcessor hatte, nur eine
            // Ebene weiter: Chase steigt aus, wenn das Ziel ein Verbuendeter (unten) oder tot bzw.
            // ungueltig ist. Verfolgt eine Einheit aber ein LEBENDES, nur unerreichbares Ziel, ist
            // bIsTargetActive wahr, kein Ausstieg greift, und sie laeuft endlos ohne anzukommen.
            // Genau das passiert hier staendig: ein Nahkaempfer (Reichweite 200) jagt einen
            // Fernkaempfer (Reichweite 900), der im Rueckzug feuert - das Ziel bleibt dauerhaft
            // gueltig und wird nie erreicht. Deshalb VOR allen Ausstiegszweigen pruefen.
            // Werte bewusst identisch zum RunStall-Waechter (6 s / 25 Einheiten).
            {
                // FORTSCHRITT HEISST NAEHER AM ZIEL, nicht "irgendwie bewegt".
                // Die erste Fassung mass die rohe Verschiebung der Einheit gegen
                // LastProgressLocation. Genau daran ist sie gescheitert: eine Einheit, die vor
                // dem Gegner hin- und herzappelt, verschiebt sich jeden Takt um mehr als die
                // Schwelle und setzt den Timer damit dauernd zurueck - der Waechter konnte nie
                // ausloesen. Gemessen am 2026-08-14: Chase stellte den mit Abstand groessten
                // Anteil der "laeuft auf der Stelle"-Meldungen, und dieselbe Einheit wurde an
                // derselben Stelle immer wieder gemeldet.
                const float ChaseStallTimeout = 6.f;
                const float ChaseStallProgressDistance = 25.f;
                const FVector ChaseNow = Transform.GetLocation();
                const float DistToTarget = FVector::Dist2D(ChaseNow, TargetFrag.LastKnownLocation);

                // Zielwechsel (anderer Gegner, oder das Ziel ist weit gesprungen)? Dann ist die
                // alte Bestmarke bedeutungslos - neu ansetzen statt faelschlich Stillstand zu
                // melden. Ein Gegner, der sich normal fortbewegt, bleibt innerhalb der Toleranz
                // und wird weiterhin korrekt als "kein Fortschritt" gewertet.
                if (FVector::Dist2D(StateFrag.BestTargetRefDestination, TargetFrag.LastKnownLocation) > 1000.f)
                {
                    StateFrag.BestTargetRefDestination = TargetFrag.LastKnownLocation;
                    StateFrag.BestTargetDistance = DistToTarget;
                    StateFrag.LastProgressLocation = ChaseNow;
                    StateFrag.NoProgressTimer = 0.f;
                }
                else if (DistToTarget < StateFrag.BestTargetDistance - ChaseStallProgressDistance)
                {
                    StateFrag.BestTargetDistance = DistToTarget;
                    StateFrag.BestTargetRefDestination = TargetFrag.LastKnownLocation;
                    StateFrag.LastProgressLocation = ChaseNow;
                    StateFrag.NoProgressTimer = 0.f;
                }
                else
                {
                    StateFrag.NoProgressTimer += ExecutionInterval;
                    if (StateFrag.NoProgressTimer >= ChaseStallTimeout)
                    {
                        // TRIAGE: die drei Verdaechtigen lassen sich hier in EINER Zeile trennen.
                        //   DesiredSpeed == 0        -> der Bewegungsbefehl wurde gar nicht gesetzt
                        //   DesiredSpeed > 0, v ~ 0  -> Befehl da, aber blockiert (Kollision/Separation/Nav)
                        //   v > 0, kein Fortschritt  -> sie bewegt sich, laeuft aber im Kreis/hin und her
                        // Ausserdem: Abstand Bewegungsziel <-> Gegner. Weichen die auseinander, zeigt der
                        // Befehl gar nicht auf den Gegner.
                        const float Tempo = FVector::Dist2D(ChaseNow, StateFrag.LastProgressLocation);
                        const float SollTempo = bHasMoveTarget ? MoveTargetList[i].DesiredSpeed.Get() : -1.f;
                        const float ZielAbw = bHasMoveTarget
                            ? FVector::Dist2D(MoveTargetList[i].Center, TargetFrag.LastKnownLocation) : -1.f;

                        // Navigationslage: trennt "kein Pfad gefunden" von "Pfad da, Bewegung
                        // wird nicht ausgefuehrt". -1 heisst: das Fragment fehlt der Entity ganz.
                        int32 PfadPunkte = -1;
                        int32 PfadIndex = -1;
                        int32 SuchtGerade = -1;
                        float NaechsterWP = -1.f;
                        if (bHasNavPathFrag)
                        {
                            const FUnitNavigationPathFragment& Nav = NavPathList[i];
                            SuchtGerade = Nav.bIsPathfindingInProgress ? 1 : 0;
                            PfadIndex = Nav.CurrentPathPointIndex;
                            if (Nav.CurrentPath.IsValid())
                            {
                                const TArray<FNavPathPoint>& Punkte = Nav.CurrentPath->GetPathPoints();
                                PfadPunkte = Punkte.Num();
                                if (Punkte.IsValidIndex(Nav.CurrentPathPointIndex))
                                {
                                    NaechsterWP = FVector::Dist2D(ChaseNow, Punkte[Nav.CurrentPathPointIndex].Location);
                                }
                            }
                            else
                            {
                                PfadPunkte = 0;
                            }
                        }

                        UE_LOG(LogTemp, Warning,
                            TEXT("[ChaseStall] bei (%.0f, %.0f) DistZiel=%.0f Reichweite=%.0f SollTempo=%.0f Versatz=%.0f ZielBefehlAbw=%.0f Pfadpunkte=%d Index=%d NaechsterWP=%.0f Sucht=%d"),
                            ChaseNow.X, ChaseNow.Y, DistToTarget,
                            Stats.AttackRange, SollTempo, Tempo, ZielAbw,
                            PfadPunkte, PfadIndex, NaechsterWP, SuchtGerade);

                        StateFrag.NoProgressTimer = 0.f;
                        StateFrag.BestTargetDistance = TNumericLimits<float>::Max();
                        StateFrag.LastProgressLocation = ChaseNow;
                        StateFrag.StoredLocation = ChaseNow;
                        StateFrag.SwitchingState = true;
                        if (bHasMoveTarget)
                        {
                            StopMovement(MoveTargetList[i], World);
                        }
                        if (SignalSubsystem)
                        {
                            SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                        }
                        continue;
                    }
                }
            }

            if (bIsFriendlyActive)
            {
                StateFrag.StoredLocation = Transform.GetLocation();
                StateFrag.SwitchingState = true;

                if (bHasMoveTarget)
                {
                    StopMovement(MoveTargetList[i], World);
                }

                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                }
                continue;
            }

            if (!bIsTargetActive || (!TargetFrag.bHasValidTarget && !StateFrag.SwitchingState))
            {
                // Queue signal instead of sending directly
                if (bHasMoveTarget)
                {
                    // Fallback to current location if StoredLocation is zero
                    FVector FinalTarget = StateFrag.StoredLocation.IsNearlyZero() ? Transform.GetLocation() : StateFrag.StoredLocation;
                    UpdateMoveTarget(
                     MoveTargetList[i],
                     FinalTarget,
                     Stats.RunSpeed,
                     World);
                }
                
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SetUnitStatePlaceholder, Entity);
                }
                continue;
            }

            // (Watchdog laeuft jetzt weiter oben, vor den Ausstiegszweigen.)

            // --- Distance Check ---

            const FMassAgentCharacteristicsFragment* TargetCharFrag = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity) ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
            const FTransformFragment* TargetTransformFrag = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity) ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
            const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

            // Lebendes Ziel = seine jetzige Position ist die letzte bekannte. Ohne das
            // verfolgte die Einheit einen eingefrorenen Punkt, hielt sich dort fuer
            // angekommen und blieb in Chase stehen; ausfuehrliche Begruendung in
            // UPauseStateProcessor::ServerExecute.
            if (TargetTransform)
            {
                const_cast<FMassAITargetFragment&>(TargetFrag).LastKnownLocation = TargetTransform->GetLocation();
            }

            const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

            const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
            const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;

            // Chase haelt bewusst ETWAS INNERHALB der Reichweite an, nicht genau auf ihrer Kante.
            //
            // Vorher war die Ankunftsschwelle hier exakt EffectiveAttackRange - und
            // UPauseStateProcessor schickt die Einheit wieder los, sobald Dist > EffectiveAttackRange.
            // Das sind komplementaere Schwellen OHNE Totband: eine Einheit, die genau auf der Kante
            // zum Stehen kommt, kippt bei jedem Ruckeln der Distanz zwischen Chase und Pause hin und
            // her. Weil sich beide Gegner bewegen, schwankt Dist staendig um diesen Punkt - genau das
            // sichtbare "Zittern", wenn zwei Einheiten sich begegnen.
            //
            // Die Loesung gehoert auf DIESE Seite: die Pause-Schwelle darf nicht aufgeweicht werden
            // (mit BreakOffRange dort entstand frueher der umgekehrte Fehler - Einheiten parkten
            // dauerhaft auf Abstand, siehe Kommentar in PauseStateProcessor). Wer naeher herangeht
            // als noetig, erzeugt dagegen ein sauberes Totband: Chase stoppt bei 0.9 * Reichweite,
            // Pause greift erst ab 1.0 * Reichweite - dazwischen passiert nichts.
            const float ArrivalRange = EffectiveAttackRange * FMath::Clamp(ChaseArrivalRangeFactor, 0.1f, 1.f);
            const float AttackRangeSq = FMath::Square(ArrivalRange);

            // --- In Attack Range ---
            if (DistSq <= AttackRangeSq && !StateFrag.SwitchingState)
            {
                if (bHasMoveTarget)
                {
                    StopMovement(MoveTargetList[i], World);
                }
                
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Pause, Entity);
                }
                StateFrag.SwitchingState = true;
                continue;
            }
            
           FVector TargetLocation = TargetFrag.LastKnownLocation;

           const FMassAgentCharacteristicsFragment* TargetCharFragPtr = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity) ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
           if (TargetCharFragPtr  && !Stats.bCanMoveWhileAttacking) //  && !Stats.bCanMoveWhileAttacking
           {
               TargetLocation.Z = TargetCharFragPtr->LastGroundLocation;
           }

           if (bHasMoveTarget)
           {
               UpdateMoveTarget(MoveTargetList[i], TargetLocation, Stats.RunSpeed, World);
           }
        }
    }); // End ForEachEntityChunk
}

void UChaseStateProcessor::SwitchToPlaceholderState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag, AActor* UnitActor)
{
    auto& Defer = Context.Defer();

    if (StateFrag.CanAttack && StateFrag.IsInitialized)
    {
        Defer.AddTag<FMassStateDetectTag>(Entity);
    }
    
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        if (FMassClientPredictionFragment* Pred = EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
        {
            // The prediction IS the client's local move target: UUnitMovementProcessor steers to Pred.Location
            // and URunStateProcessor::ExecuteClient measures ARRIVAL against it while bHasData is set. Pinning it
            // to the current transform means "stop here" - correct only for a placeholder that does NOT move.
            // With PlaceholderSignal == Run we add FMassStateRunTag below, so the pin would make the unit measure
            // its distance to ITSELF, "arrive" on the very next Run tick and fall back to Idle where it stands.
            //
            // Resume target for a Run placeholder = StateFrag.StoredLocation - exactly what ExecuteServer resumes
            // to on this same lost-target branch, so client and server converge. Do NOT reuse the incoming
            // Pred.Location: the chase loop above overwrites it with TargetFrag.LastKnownLocation every tick, so
            // it points at the enemy we just lost, not at the commanded destination.
            if (StateFrag.PlaceholderSignal == UnitSignals::Run)
            {
                FVector ResumeLocation = StateFrag.StoredLocation;
                if (ResumeLocation.IsNearlyZero())
                {
                    // A Run placeholder mirrored from the replicated actor state never sets StoredLocation on the
                    // client - fall back to the replicated server target, as UPauseStateProcessor::ClientExecute
                    // does for its own Run placeholder.
                    if (const FMassMoveTargetFragment* MoveTargetFrag = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
                    {
                        ResumeLocation = MoveTargetFrag->Center;
                    }
                }

                if (!ResumeLocation.IsNearlyZero())
                {
                    Pred->Location = ResumeLocation;
                    if (const FMassCombatStatsFragment* StatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
                    {
                        Pred->PredDesiredSpeed = StatsPtr->RunSpeed;
                    }
                    Pred->bHasData = true;
                    Pred->PredSource = 24; // [PredDiag]
                }
                else if (const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
                {
                    // Nothing sane to resume to - keep the stop-here behaviour rather than steer to world origin.
                    Pred->Location = TF->GetTransform().GetLocation();
                    Pred->PredDesiredSpeed = 0.f;
                    Pred->bHasData = true;
                    Pred->PredSource = 25; // [PredDiag]
                }
            }
            else
            {
                if (const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
                {
                    Pred->Location = TF->GetTransform().GetLocation();
                }
                Pred->PredDesiredSpeed = 0.f;
                Pred->bHasData = true;
                Pred->PredSource = 26; // [PredDiag]
            }
        }

        StateFrag.SwitchingStateClient = true;
        
        // --- PRÄZISES TAG-MANAGEMENT ---
        // Entferne nur die Tags, die definitiv NICHT dem Ziel-Zustand entsprechen
        if (StateFrag.PlaceholderSignal != UnitSignals::Run) Defer.RemoveTag<FMassStateRunTag>(Entity);
        if (StateFrag.PlaceholderSignal != UnitSignals::PatrolRandom) Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
        if (StateFrag.PlaceholderSignal != UnitSignals::PatrolIdle) Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
        if (StateFrag.PlaceholderSignal != UnitSignals::Idle) Defer.RemoveTag<FMassStateIdleTag>(Entity);

        // Diese Tags können immer entfernt werden, da sie nicht als Placeholder dienen
        Defer.RemoveTag<FMassStateChaseTag>(Entity);
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        Defer.RemoveTag<FMassStatePauseTag>(Entity);
        Defer.RemoveTag<FMassStateCastingTag>(Entity);
        Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
        Defer.RemoveTag<FMassStateBuildTag>(Entity);
        Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
        Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);

        // Prediction: Setzen des korrekten Placeholder-Tags basierend auf dem Signal
        if (StateFrag.PlaceholderSignal == UnitSignals::PatrolRandom)
        {
            Defer.AddTag<FMassStatePatrolRandomTag>(Entity);
        }
        else if (StateFrag.PlaceholderSignal == UnitSignals::PatrolIdle)
        {
            Defer.AddTag<FMassStatePatrolIdleTag>(Entity);
        }
        else if (StateFrag.PlaceholderSignal == UnitSignals::Run)
        {
            Defer.AddTag<FMassStateRunTag>(Entity);
        }
        else
        {
            Defer.AddTag<FMassStateIdleTag>(Entity);
        }
        
        // Lokale Actor-Synchronisation (UnitStatePlaceholder)
        if (AUnitBase* UnitBase = Cast<AUnitBase>(UnitActor))
        {
            if (StateFrag.PlaceholderSignal == UnitSignals::PatrolRandom) UnitBase->UnitStatePlaceholder = UnitData::Patrol;
            else if (StateFrag.PlaceholderSignal == UnitSignals::PatrolIdle) UnitBase->UnitStatePlaceholder = UnitData::PatrolIdle;
            else if (StateFrag.PlaceholderSignal == UnitSignals::Run) UnitBase->UnitStatePlaceholder = UnitData::Run;
            else UnitBase->UnitStatePlaceholder = UnitData::Idle;
        }
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SetUnitStatePlaceholder, Entity);
        }
    }
}

// void UChaseStateProcessor::CalculateRadii...
