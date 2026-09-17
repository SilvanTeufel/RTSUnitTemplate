// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/PauseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
#include "Core/RTSUnitUtils.h"
#include "Async/Async.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Mass/Projectile/ProjectileVisualManager.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassReplicationFragments.h"
#include "Actors/Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Mass/States/CombatPlaceholder.h"

UPauseStateProcessor::UPauseStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UPauseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All); // Nur Pause-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben, Zustand ändern
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Stats lesen (AttackPauseDuration)
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite); // Eigene Position für Distanzcheck
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    //EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UPauseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());

    if (SignalSubsystem)
    {
        // Bindung der Funktion an das Signal "RangedAttack"
        ProjectileSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RangedAttack)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UPauseStateProcessor, OnProjectileSignalReceived));
    }
}

void UPauseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    UWorld* World = EntityManager.GetWorld();
    if (!World || !SignalSubsystem) return;

    const bool bIsClient = (World->GetNetMode() == NM_Client);

    EntityQuery.ForEachEntityChunk(Context, [this, bIsClient, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        auto ForceList = ChunkContext.GetMutableFragmentView<FMassForceFragment>();
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i, ActorList[i].GetMutable());
            }
            else
            {
                ServerExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i, ActorList[i].GetMutable());
            }
        }
    });

}

void UPauseStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World) return;

    // Haengengebliebenes SwitchingState wieder loesen, bevor die Ausgaenge geprueft
    // werden - sonst bleibt die Einheit dauerhaft in Pause stehen. Siehe
    // RTSUnitUtils::TickSwitchingStateWatchdog.
    RTSUnitUtils::TickSwitchingStateWatchdog(StateFrag, ExecutionInterval);

    FMassAITargetFragment& MutableTargetFrag = const_cast<FMassAITargetFragment&>(TargetFrag);

    if (!StateFrag.CanAttack)
    {
        MutableTargetFrag.TargetEntity.Reset();
        MutableTargetFrag.bHasValidTarget = false;
    }

    bool bIsTargetActive = RTSUnitUtils::IsEntityUsable(EntityManager, MutableTargetFrag.TargetEntity);

    // Guard und Fremdzugriff stehen bewusst DIREKT beieinander. Vorher lagen die vier Zeilen
    // fuer Friendly-Pruefung, Transform- und Charakteristik-Sicht dazwischen; ein Absturz am
    // 15.08.2026 traf genau diesen Zugriff:
    //   Assertion failed: CurrentArchetype [MassEntityManager.cpp:2367]
    // Das ist der ungepruefte Pfad (InternalGetFragmentDataPtr) - er prueft NICHT, ob die
    // Entitaet noch aktiv ist, sondern assertet direkt auf dem Archetyp. Der Guard eine
    // Handvoll Zeilen vorher genuegt dafuer nicht; dieselbe Lehre steht weiter unten vor
    // TargetCharFrag. Der Absturz lag auf dem GameThread, nicht in einem Worker - die
    // GameThread-Bindung im Konstruktor schuetzt hier also nichts.
    FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive
        ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MutableTargetFrag.TargetEntity)
        : nullptr;
    const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

    const bool bIsFriendlyActive = RTSUnitUtils::IsEntityUsable(EntityManager, MutableTargetFrag.FriendlyTargetEntity);
    const auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment* CharFragPtr = CharList.IsValidIndex(EntityIdx) ? &CharList[EntityIdx] : nullptr;

    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIdx];
    
    // Eine bereits angefangene Pause wird zu Ende gebracht, auch wenn das Ziel dazwischen
    // stirbt. Hier faellt der Schuss der Projektil-Einheiten (RangedAttack am Ende der
    // Pause); brach der Zustand vorher ab, ging StateTimer auf 0 und eine Einheit mit langer
    // PauseDuration - Siege-Kanone: 3 s - fing beim naechsten Ziel wieder von vorn an und
    // kam nie zum Schuss.
    const bool bPauseInProgress = bFinishPauseOnTargetLoss
        && Stats.bUseProjectile
        && StateFrag.StateTimer > 0.f
        && StateFrag.StateTimer < Stats.PauseDuration;

    const bool bLostTarget = !bIsTargetActive || !MutableTargetFrag.bHasValidTarget || bIsTargetDead;
    const bool bFriendlyOutOfRange = bIsFriendlyActive
        && !RTSUnitUtils::IsWithinFollowThreshold(EntityManager, Entity, MutableTargetFrag,
            CharFragPtr, Transform.GetLocation(), MoveTarget, World, FollowAcceptanceMultiplier);

    if (bFriendlyOutOfRange || (bLostTarget && !bPauseInProgress))
    {
        if (!StateFrag.SwitchingState)
        {
            StateFrag.SwitchingState = true;
            
            RTSUnitUtils::ResolvePlaceholderAfterCombat(StateFrag, Actor);

            auto& Defer = Context.Defer();
            if (StateFrag.CanAttack && StateFrag.IsInitialized)
            {
                Defer.AddTag<FMassStateDetectTag>(Entity);
            }
            
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SetUnitStatePlaceholder, Entity);
            }
        }
        return;
    }

    StateFrag.StateTimer += ExecutionInterval;

    // bIsTargetActive stammt aus dem Anfang dieser Funktion. Zwischen dort und hier liegen
    // ueber fuenfzig Zeilen mit Deferred Commands und einem SignalEntityDeferred. Stirbt das
    // Ziel in diesem Fenster oder wechselt es seinen Archetyp, behauptet das alte Flag
    // weiterhin "aktiv" und der Zugriff unten assertet:
    //   Assertion failed: CurrentArchetype [MassEntityManager.cpp:2367]
    // Die GameThread-Bindung (bRequiresGameThreadExecution im Konstruktor) verhindert das
    // NICHT - sie schuetzt gegen fremde Threads, nicht gegen einen veralteten eigenen Guard.
    // Deshalb unmittelbar vor dem Zugriff neu pruefen.
    bIsTargetActive = bIsTargetActive && RTSUnitUtils::IsEntityUsable(EntityManager, MutableTargetFrag.TargetEntity);

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    const FMassAgentCharacteristicsFragment& CharFrag = *CharFragPtr;
    FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(MutableTargetFrag.TargetEntity) : nullptr;
    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

    // Solange das Ziel lebt und greifbar ist, IST seine jetzige Position die letzte
    // bekannte. Ohne diese Auffrischung fror LastKnownLocation ein, sobald das Ziel aus
    // der Sicht lief. Die Einheit mass dann gegen einen Geisterpunkt direkt neben sich,
    // hielt sich fuer in Reichweite und blieb dauerhaft in Pause -> Attack -> Pause
    // stehen, waehrend der echte Gegner ueber 1000 Einheiten entfernt war. Gemessen:
    // sechs Einheiten 12 s bewegungslos, naechster Gegner 947 bis 2357 Einheiten weg.
    if (bIsTargetActive && TargetTransform)
    {
        MutableTargetFrag.LastKnownLocation = TargetTransform->GetLocation();
    }

    const float Dist = FVector::Dist2D(Transform.GetLocation(), MutableTargetFrag.LastKnownLocation);

    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, MutableTargetFrag.LastKnownLocation);
    const float AttackRange = Stats.AttackRange + CombinedRadii;

    // Gleiches Totband wie im AttackStateProcessor: Eintritt bei AttackRange, Austritt erst
    // bei AttackRange * Hysterese. Ohne das verlor eine Einheit an der Reichweitengrenze bei
    // jedem Flackern ihren angefangenen Angriffszyklus.
    const float BreakOffRange = AttackRange * FMath::Max(1.f, AttackRangeHysteresis);

    auto GoAfterTarget = [this, &Context, &Stats, Entity]()
    {
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context,
                Stats.bCanMoveWhileAttacking ? UnitSignals::Run : UnitSignals::Chase, Entity);
        }
    };

    if (StateFrag.StateTimer >= Stats.PauseDuration && !StateFrag.SwitchingState)
    {
        // Abklingzeit vorbei - jetzt wird neu entschieden, und dafuer zaehlt die ECHTE
        // Reichweite, nicht das Hysteresefenster.
        //
        // Mit BreakOffRange an dieser Stelle entstand eine Endlosschleife: im Ring zwischen
        // AttackRange und AttackRange * Hysterese schickte Pause die Einheit wieder in den
        // Angriff, AttackStateProcessor liess sie im selben erweiterten Fenster gewaehren
        // und schickte sie zurueck in die Pause. Die Einheit stand damit dauerhaft auf
        // Abstand zum Gegner, statt das letzte Stueck heranzugehen - am deutlichsten bei
        // kurzen Reichweiten wie dem Skitterling, wo dieser Abstand ins Auge faellt.
        //
        // Das gilt ausdruecklich AUCH fuer Fernkaempfer. Ein Ausnahmezweig, der hier
        // BreakOffRange verwendete, hat den Skitterling weiter haengen lassen: der
        // Entschluss rechnet mit AttackRange + CombinedRadii - also inklusive Radius des
        // ZIELS -, die Abschlusspruefung in UnitRangedAttack dagegen mit RangeWithCapsule.
        // Eine kleine Einheit vor einem grossen Gegner durfte damit schiessen wollen, ohne
        // dass der Schuss die Pruefung bestand: Pause, kein Schuss, Pause. Der Entschluss
        // muss enger sein als die Abschusspruefung, nie weiter.
        StateFrag.SwitchingState = true;

        // Kleiner ABSOLUTER Zuschlag als Totband gegen das Hin- und Herkippen:
        // Chase meldet "angekommen" bei Dist <= AttackRange, hier wird ab Dist > AttackRange
        // wieder losgeschickt. Ohne Zuschlag sind das komplementaere Schwellen - eine Einheit
        // genau auf der Kante wechselt bei jeder Distanzschwankung, und weil sich beide Gegner
        // bewegen, schwankt die Distanz staendig.
        //
        // Bewusst ABSOLUT und klein, nicht multiplikativ: ein Faktor auf der Chase-Seite
        // (ChaseArrivalRangeFactor < 1) verlangt physisches Naeherkommen, das Kollision und
        // Separation verhindern koennen - dann jagt die Einheit ewig und laeuft auf der Stelle.
        // Ein Faktor HIER (BreakOffRange) war der frueher dokumentierte Fehler in die andere
        // Richtung: bei Reichweite 900 sind 15 % ganze 135 Einheiten, und die Einheiten parkten
        // sichtbar auf Abstand. 25 Einheiten sind kleiner als jede Angriffsreichweite im Spiel
        // und faellt optisch nicht auf.
        if (Dist > AttackRange + PauseRechaseEpsilon)
        {
            GoAfterTarget();
        }
        else if (Stats.bUseProjectile)
        {
            if (SignalSubsystem)
            {
                StateFrag.StateTimer = 0.f;
                // Bei SpawnProjectileAtPercentage > 0 wechselt die Einheit hier nur in den
                // Angriff (dasselbe Signal wie im Nahkampf) - geschossen wird erst im Verlauf
                // der Angriffsanimation, siehe UAttackStateProcessor. Bei 0 faellt der Schuss
                // wie bisher schon beim Eintritt in den Angriff.
                SignalSubsystem->SignalEntityDeferred(
                    Context,
                    Stats.SpawnProjectileAtPercentage > 0.f ? UnitSignals::Attack : UnitSignals::RangedAttack,
                    Entity);
            }
        }
        else
        {
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Attack, Entity);
            }
        }
    }
    else if (Dist > BreakOffRange && !StateFrag.SwitchingState)
    {
        // Waehrend der laufenden Abklingzeit erst jenseits des Totbands abbrechen - das ist
        // der Fall, fuer den die Hysterese gedacht war.
        StateFrag.SwitchingState = true;
        GoAfterTarget();
    }
}

void UPauseStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor)
{
    if (StateFrag.SwitchingStateClient)
    {
        StateFrag.SwitchingStateClient = false;
    }


    // === BatchDiag (TEMP): Pause client entry. If a recently-commanded unit still shows here with
    // Pause tag, the move command did not strip Pause and ApplyAttackStopLogic will freeze it. ===
    if (!Stats.bCanMoveWhileAttacking)
    {
        RTS_BatchDiagLog(TEXT("PAUSE-CLIENT(freeze)"), EntityManager.GetWorld(), EntityManager, Entity,
            Cast<AUnitBase>(Actor) ? Cast<AUnitBase>(Actor)->UnitIndex : -1,
            EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity));
    }

    ApplyAttackStopLogic(Context, Stats, TargetFrag, Entity, EntityIdx);

    StateFrag.StateTimerClient += ExecutionInterval;
    
    const auto TransformList = Context.GetFragmentView<FTransformFragment>();
    const FTransform& Transform = TransformList[EntityIdx].GetTransform();
    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];

    bool bIsTargetActive = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity);
    const bool bIsFriendlyActive = RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.FriendlyTargetEntity);
    auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
    FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIdx];

    if (bIsFriendlyActive && !RTSUnitUtils::IsWithinFollowThreshold(EntityManager, Entity, TargetFrag, &CharFrag, Transform.GetLocation(), MoveTarget, EntityManager.GetWorld(), FollowAcceptanceMultiplier))
    {
        if (!StateFrag.SwitchingStateClient)
        {
            StateFrag.SwitchingStateClient = true;
            StateFrag.StateTimerClient = 0.f;
            auto& Defer = Context.Defer();

            RTSUnitUtils::ResolvePlaceholderAfterCombat(StateFrag, Actor);

            if (StateFrag.CanAttack && StateFrag.IsInitialized)
            {
                Defer.AddTag<FMassStateDetectTag>(Entity);
            }
            Defer.RemoveTag<FMassStatePauseTag>(Entity);
            Defer.AddTag<FMassStateIdleTag>(Entity);
        }
        return;
    }

    // Server-Paritaet (siehe ServerExecute): ein Ziel mit Health <= 0 gilt als verloren. Auf dem Client
    // bleibt die Leichen-Entity aktiv (TargetEntity wird auf dem Client bewusst nie zurueckgesetzt), also
    // reicht IsEntityActive() hier nicht - sonst steht die Einheit eine volle PauseDuration vor der Leiche.
    // Gleiches Muster wie UAttackStateProcessor::ClientExecute.
    FMassCombatStatsFragment* TgtStatsPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity) : nullptr;
    const bool bIsTargetDead = TgtStatsPtr && TgtStatsPtr->Health <= 0.f;

    if (bIsTargetDead)
    {
        if (!StateFrag.SwitchingStateClient)
        {
            StateFrag.SwitchingStateClient = true;
            StateFrag.StateTimerClient = 0.f;

            RTSUnitUtils::ResolvePlaceholderAfterCombat(StateFrag, Actor);

            auto& Defer = Context.Defer();
            if (StateFrag.CanAttack && StateFrag.IsInitialized)
            {
                Defer.AddTag<FMassStateDetectTag>(Entity);
            }
            Defer.RemoveTag<FMassStatePauseTag>(Entity);
            Defer.AddTag<FMassStateIdleTag>(Entity);
        }
        return;
    }

    // Gleiche Falle wie im Server-Pfad: der Guard ist am Funktionsanfang berechnet, dazwischen
    // liegen Deferred Commands. Vor dem Fremdzugriff neu pruefen, sonst assertet
    // CurrentArchetype (MassEntityManager.cpp:2367).
    bIsTargetActive = bIsTargetActive && RTSUnitUtils::IsEntityUsable(EntityManager, TargetFrag.TargetEntity);

    FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
    FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

    const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
    
    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
    const float AttackRange = Stats.AttackRange + CombinedRadii;

    if (bIsTargetActive)
    {
        if (Dist <= AttackRange)
        {
            if (StateFrag.StateTimerClient >= Stats.PauseDuration)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    StateFrag.SwitchingStateClient = true;
                    StateFrag.StateTimerClient = 0.f;
                    auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
                    if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
                    {
                        if (!Stats.bCanMoveWhileAttacking)
                        {
                            FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
                            Pred.Location = TargetFrag.LastKnownLocation;
                            Pred.PredDesiredSpeed = 0.f;
                            Pred.bHasData = true;
                            Pred.PredSource = 12; // [PredDiag]
                        }
                    }
                    Context.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                    Context.Defer().AddTag<FMassStateAttackTag>(Entity);
                }
            }
        }
        else if (Dist > AttackRange + 150.f)
        {
            if (!StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = true;
                StateFrag.StateTimerClient = 0.f;
                auto& Defer = Context.Defer();
                auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
                if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
                {
                    FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
                    if (const FMassMoveTargetFragment* MoveTargetFrag = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
                    {
                        Pred.Location = MoveTargetFrag->Center;
                    }
                    else
                    {
                        Pred.Location = TargetFrag.LastKnownLocation;
                    }
                    Pred.PredDesiredSpeed = Stats.RunSpeed;
                    Pred.bHasData = true;
                    Pred.PredSource = 13; // [PredDiag]
                }
                if (StateFrag.CanAttack && StateFrag.IsInitialized)
                {
                    Defer.AddTag<FMassStateDetectTag>(Entity);
                }
                Defer.RemoveTag<FMassStatePauseTag>(Entity);
                
                if (Stats.bCanMoveWhileAttacking)
                {
                    Defer.AddTag<FMassStateRunTag>(Entity);
                }
                else
                {
                    Defer.AddTag<FMassStateChaseTag>(Entity);
                }
            }
        }
    }
    else
    {
        if (!StateFrag.SwitchingStateClient)
        {
            StateFrag.SwitchingStateClient = true;
            StateFrag.StateTimerClient = 0.f;
            auto& Defer = Context.Defer();
            auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
            if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
            {
                FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
                if (StateFrag.PlaceholderSignal == UnitSignals::Run)
                {
                    if (const FMassMoveTargetFragment* MoveTargetFrag = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
                    {
                        Pred.Location = MoveTargetFrag->Center;
                    }
                    Pred.PredDesiredSpeed = Stats.RunSpeed;
                }
                else
                {
                    Pred.Location = Transform.GetLocation();
                    Pred.PredDesiredSpeed = 0.f;
                }
                Pred.bHasData = true;
                Pred.PredSource = 14; // [PredDiag]
            }
            
            Defer.RemoveTag<FMassStatePauseTag>(Entity);
            
            if (StateFrag.PlaceholderSignal == UnitSignals::Run)
                Defer.AddTag<FMassStateRunTag>(Entity);
            else
                Defer.AddTag<FMassStateIdleTag>(Entity);
        }
    }
}

void UPauseStateProcessor::ApplyAttackStopLogic(FMassExecutionContext& Context, 
    const FMassCombatStatsFragment& Stats, const FMassAITargetFragment& TargetFrag, 
    const FMassEntityHandle Entity, const int32 EntityIdx)
{
    if (!Stats.bCanMoveWhileAttacking)
    {
        auto VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
        auto ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
        auto PredictionList = Context.GetMutableFragmentView<FMassClientPredictionFragment>();
        auto MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto TransformList = Context.GetFragmentView<FTransformFragment>();

        if (VelocityList.IsValidIndex(EntityIdx)) VelocityList[EntityIdx].Value *= 0.1f;
        if (ForceList.IsValidIndex(EntityIdx)) ForceList[EntityIdx].Value = FVector::ZeroVector;

        if (PredictionList.Num() > 0 && PredictionList.IsValidIndex(EntityIdx))
        {
            FMassClientPredictionFragment& Pred = PredictionList[EntityIdx];
            Pred.Location = TransformList[EntityIdx].GetTransform().GetLocation();
            Pred.PredDesiredSpeed = 0.f;
            Pred.bHasData = true;
            Pred.PredSource = 15; // [PredDiag]
        }
    }
}

// float UPauseStateProcessor::GetCombinedRadii...

void UPauseStateProcessor::OnProjectileSignalReceived(FName SignalName, const TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem) return;
	
	UWorld* World = EntitySubsystem->GetWorld();
	if (!World || World->GetNetMode() != NM_Client) return; // FIX: Nur auf dem Client spawnen (Server spawnt via UnitBase)

	// Schutz gegen Mehrfach-Instanzen: Nur ein Spawn pro Entity pro Frame
	static TMap<FMassEntityHandle, uint64> LastSpawnFramePerEntity;
	const uint64 CurrentFrame = GFrameCounter;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (EntityManager.IsEntityValid(Entity))
		{
			if (uint64* LastFrame = LastSpawnFramePerEntity.Find(Entity))
			{
				if (*LastFrame == CurrentFrame) continue; // Bereits in diesem Frame gespawnt
			}
			LastSpawnFramePerEntity.Add(Entity, CurrentFrame);

			ExecuteProjectileSpawn(EntityManager, Entity);
		}
	}

	// Aufräumen alter Einträge
	if (LastSpawnFramePerEntity.Num() > 2000)
	{
		for (auto It = LastSpawnFramePerEntity.CreateIterator(); It; ++It)
		{
			if (It.Value() < CurrentFrame) It.RemoveCurrent();
		}
	}
}

void UPauseStateProcessor::ExecuteProjectileSpawn(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
{
    // 1. Daten aus Fragmenten abrufen
    const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
    const FMassAgentCharacteristicsFragment* AC = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
    FMassActorFragment* AF = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    const FMassAITargetFragment* TargetF = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);
    const FMassCombatStatsFragment* StatsF = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

    if (!TF || !AC || !AF || !TargetF || !StatsF) return;

    AUnitBase* UnitActor = Cast<AUnitBase>(AF->GetMutable());
    if (!UnitActor || !UnitActor->ProjectileBaseClass) return;

    UWorld* World = UnitActor->GetWorld();
    if (!World) return;
    UProjectileVisualManager* VisualManager = World->GetSubsystem<UProjectileVisualManager>();
    if (!VisualManager) return;

    const AProjectile* ProjCDO = VisualManager->GetProjectileCDO(UnitActor->ProjectileBaseClass);
    if (!ProjCDO) return;

    // 2. Positionsbestimmung: Nutze direkt die Actor-Logik für den Mündungs-Punkt
    FVector SpawnLocation = UnitActor->GetProjectileSpawnLocation();
    
    FTransform BaseSpawnXf = TF->GetTransform();
    BaseSpawnXf.SetLocation(SpawnLocation);

    const FVector BaseSpawnPos = SpawnLocation;
    
    // 3. Multi-Shot Logik aus CDO (Twin Projectiles / Homing Salven)
    int32 HomingCount = ProjCDO->HomingMissleCount;
    int32 BaseCount = (HomingCount > 0) ? HomingCount : 1;
    TArray<FVector> SpawnPositions;

    if (ProjCDO->TwinProjectileDistance >= 10.f)
    {
        FVector DirToTarget = (TargetF->LastKnownLocation - BaseSpawnPos).GetSafeNormal2D();
        FVector RightVector = DirToTarget.IsNearlyZero() ? UnitActor->GetActorRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
        FVector RightOffset = RightVector * ProjCDO->TwinProjectileDistance;
        SpawnPositions.Add(BaseSpawnPos - RightOffset);
        SpawnPositions.Add(BaseSpawnPos + RightOffset);
    }
    else
    {
        SpawnPositions.Add(BaseSpawnPos);
    }

    // 4. Finaler Spawn über VisualManager
    float ProjectileSpeed = (UnitActor->Attributes && UnitActor->Attributes->GetProjectileSpeed() > 0.f) ? UnitActor->Attributes->GetProjectileSpeed() : ProjCDO->MovementSpeed;

    for (const FVector& Pos : SpawnPositions)
    {
        for (int32 i = 0; i < BaseCount; ++i)
        {
            FTransform FinalSpawnXf = BaseSpawnXf;
            FinalSpawnXf.SetLocation(Pos);
            
            FVector TargetLoc = TargetF->LastKnownLocation;
            FVector Direction = (TargetLoc - Pos).GetSafeNormal();
            float InitialAngle = 0.f;
            float RotSpeed = ProjCDO->HomingRotationSpeed;
            float MaxRadius = ProjCDO->HomingMaxSpiralRadius;
            float FinalSpeed = ProjectileSpeed;

            if (HomingCount > 0)
            {
                // Richtungs-Jitter für fächerförmigen Start (Identisch zum Server)
                if (BaseCount > 1)
                {
                    float JitterAngle = (360.0f / BaseCount) * i;
                    FVector Right, Up;
                    Direction.FindBestAxisVectors(Right, Up);
                    Direction = (Direction + (Right * FMath::Cos(FMath::DegreesToRadians(JitterAngle)) + Up * FMath::Sin(FMath::DegreesToRadians(JitterAngle))) * 0.1f).GetSafeNormal();
                }
                InitialAngle = FMath::RandRange(0.f, 360.f);
                RotSpeed *= FMath::RandRange(0.9f, 1.4f);
                if (FMath::RandBool()) RotSpeed *= -1.f;
                MaxRadius *= FMath::RandRange(0.8f, 1.2f);
                FinalSpeed += FMath::RandRange(-ProjCDO->HomingSpeedVariation, ProjCDO->HomingSpeedVariation);
            }
            else
            {
                InitialAngle = (HomingCount > 1) ? (360.0f / BaseCount) * i : 0.f;
            }

            if (!Direction.IsNearlyZero()) FinalSpawnXf.SetRotation(FQuat(Direction.Rotation()));

            bool bFinalFollowTarget = ProjCDO->FollowTarget;
            if (HomingCount > 0) bFinalFollowTarget = true; // FIX: Konsistenz mit Server erzwingen

            VisualManager->SpawnMassProjectile(
                UnitActor->ProjectileBaseClass,
                FinalSpawnXf,
                UnitActor, nullptr,
                TargetLoc,
                Entity,
                TargetF->TargetEntity,
                FinalSpeed,
                StatsF->TeamId,
                bFinalFollowTarget, // FIX: bIsHoming wird dadurch im Fragment TRUE
                InitialAngle,
                RotSpeed,
                MaxRadius,
                ProjCDO->HomingInterpSpeed,
                nullptr, // Keine Deferral hier nötig, da wir im Signal-Handler sind (Game Thread)
                UnitActor->ProjectileScale,
                -1.f, // Nutze CDO-Schaden via Lazy-Init
                -1,    // Nutze CDO-MaxPiercedTargets via Lazy-Init (FIX!)
                true   // bIsPredicted
            );
        }
    }
}
