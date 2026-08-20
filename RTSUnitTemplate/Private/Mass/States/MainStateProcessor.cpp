// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/MainStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"  // nur fuer die Stall-Diagnose
#include "NavigationSystem.h"  // nur fuer die Stall-Diagnose: liegt die Einheit auf dem Navmesh?
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"

UMainStateProcessor::UMainStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UMainStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite); // Eigene Stats lesen/schreiben
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    // Nur fuer die [Stall]-Diagnose: Soll-Tempo, Restweg und Pfadzustand. Optional, damit die
    // Abfrage dieselben Entitaeten trifft wie bisher - eine Pflichtangabe wuerde den Prozessor
    // stillschweigend auf einen Teil der Einheiten einschraenken.
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FUnitNavigationPathFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    //EntityQuery.AddTagRequirement<FMassStopUnitDetectionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateNeedsInitialKickTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    
	EntityQuery.RegisterWithProcessor(*this);
}

void UMainStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UMainStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling Check ---
    TimeSinceLastRunA += Context.GetDeltaTimeSeconds();

    if (TimeSinceLastRunA < ExecutionInterval)
    {
        return; // Skip execution this frame
    }
    // Interval reached, reset timer
    TimeSinceLastRunA -= ExecutionInterval; // Or TimeSinceLastRun = 0.0f;

    // Branch by net mode
    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UMainStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Get World and Signal Subsystem (only if interval was met) ---
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager for World consistently
    if (!World) return;

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StatsList = ChunkContext.GetMutableFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable needed
        auto ImpactList = ChunkContext.GetFragmentView<FEffectAreaImpactFragment>();
        auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto MoveList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const auto NavList  = ChunkContext.GetFragmentView<FUnitNavigationPathFragment>();

        // DIAGNOSE "laeuft auf der Stelle": der Zustand steckt in Tags, und alle Einheiten eines
        // Chunks teilen sich den Archetyp - also einmal pro Chunk bestimmen, nicht pro Einheit.
        // Nur Bewegungszustaende sind interessant; wer idlet oder angreift, SOLL stehen.
        const TCHAR* StallStateName = nullptr;
        if      (ChunkContext.DoesArchetypeHaveTag<FMassStateChaseTag>())        StallStateName = TEXT("Chase");
        else if (ChunkContext.DoesArchetypeHaveTag<FMassStateRunTag>())          StallStateName = TEXT("Run");
        else if (ChunkContext.DoesArchetypeHaveTag<FMassStatePatrolRandomTag>()) StallStateName = TEXT("PatrolRandom");
        else if (ChunkContext.DoesArchetypeHaveTag<FMassStatePatrolTag>())       StallStateName = TEXT("Patrol");
        else if (ChunkContext.DoesArchetypeHaveTag<FMassStateGoToBaseTag>())     StallStateName = TEXT("GoToBase");
        else if (ChunkContext.DoesArchetypeHaveTag<FMassStateGoToBuildTag>())    StallStateName = TEXT("GoToBuild");
        else if (ChunkContext.DoesArchetypeHaveTag<FMassStateGoToResourceExtractionTag>()) StallStateName = TEXT("GoToResource");

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable ref needed
            FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FEffectAreaImpactFragment* ImpactFragPtr = ImpactList.Num() > 0 ? &ImpactList[i] : nullptr;

            HandleLoseSightExtension(StateFrag, StatsFrag);

            // DIAGNOSE "laeuft auf der Stelle" - nur protokollieren, kein Eingriff.
            if (StallStateName)
            {
                const FVector Jetzt = TransformList[i].GetTransform().GetLocation();
                if (FVector::Dist2D(Jetzt, StateFrag.StallDiagLocation) > 40.f)
                {
                    StateFrag.StallDiagLocation = Jetzt;
                    StateFrag.StallDiagTimer = 0.f;
                    StateFrag.bStallDiagReported = false;
                }
                else
                {
                    StateFrag.StallDiagTimer += ExecutionInterval;
                    if (StateFrag.StallDiagTimer >= 8.f && !StateFrag.bStallDiagReported)
                    {
                        StateFrag.bStallDiagReported = true;
                            // Scheitert die Projektion der EIGENEN Position, ist die Einheit neben dem
                            // Navigationsnetz gelandet - dann setzt UnitMovementProcessor Tempo 0 und
                            // verwirft den Pfad, und im naechsten Takt genau dasselbe. Sie muesste sich
                            // bewegen, um zurueckzukommen, bewegt sich aber ohne Pfad nicht. Diese Zahl
                            // trennt "steht selbst falsch" von "Ziel nicht erreichbar".
                            // Zwei Projektionen: die eigene Position UND das Ziel. Die erste hat sich
                            // erledigt (2 von 116 lagen daneben) - die Einheit steht richtig, die
                            // Startprojektion gelingt, die Pfadanfrage geht raus und scheitert. Bleibt
                            // die Frage, ob das ZIEL ueberhaupt auf dem Netz liegt; dann waere die
                            // Baustelle an einer unerreichbaren Stelle gesetzt worden.
                            int32 AufNetz = -1, ZielAufNetz = -1;
                            if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
                            {
                                FNavLocation Projiziert;
                                AufNetz = NavSys->ProjectPointToNavigation(
                                    Jetzt, Projiziert, FVector(500.f, 500.f, 500.f)) ? 1 : 0;
                                if (MoveList.Num() > 0)
                                {
                                    FNavLocation ZielProjiziert;
                                    ZielAufNetz = NavSys->ProjectPointToNavigation(
                                        MoveList[i].Center, ZielProjiziert, FVector(500.f, 500.f, 500.f)) ? 1 : 0;
                                }
                            }
                            float SollTempo = -1.f, RestWeg = -1.f;
                            int32 PfadPunkte = -1, PfadIndex = -1, Sucht = -1;
                            if (MoveList.Num() > 0)
                            {
                                SollTempo = MoveList[i].DesiredSpeed.Get();
                                RestWeg = MoveList[i].DistanceToGoal;
                            }
                            if (NavList.Num() > 0)
                            {
                                const FUnitNavigationPathFragment& Nav = NavList[i];
                                Sucht = Nav.bIsPathfindingInProgress ? 1 : 0;
                                PfadIndex = Nav.CurrentPathPointIndex;
                                PfadPunkte = Nav.CurrentPath.IsValid() ? Nav.CurrentPath->GetPathPoints().Num() : 0;
                            }
                        // SwitchingState mitloggen: GoToBuildStateProcessor haengt BEIDE Ausgaenge (Abbruch und
                            // Ankunft) an !SwitchingState, und der Wachhund
                            // RTSUnitUtils::TickSwitchingStateWatchdog laeuft nur in Attack/Chase/Pause.
                            // Steht das Flag, ist der Arbeiter dauerhaft eingefroren. Ohne diese Angabe
                            // laesst sich die Vermutung nicht von "steht nur im Weg" unterscheiden.
                            UE_LOG(LogTemp, Warning,
                            TEXT("[Stall] Team=%d Einheit steht seit %.0fs im Zustand %s bei (%.0f, %.0f) Wechselt=%d "
                                 "SollTempo=%.0f RestWeg=%.0f Pfadpunkte=%d Index=%d Sucht=%d AufNetz=%d ZielAufNetz=%d"),
                            StatsFrag.TeamId, StateFrag.StallDiagTimer, StallStateName, Jetzt.X, Jetzt.Y,
                            StateFrag.SwitchingState ? 1 : 0,
                            SollTempo, RestWeg, PfadPunkte, PfadIndex, Sucht, AufNetz, ZielAufNetz);
                    }
                }
            }
            else
            {
                StateFrag.StallDiagTimer = 0.f;
                StateFrag.bStallDiagReported = false;
            }

            if (StateFrag.BirthTime == TNumericLimits<float>::Max())
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UnitSpawned, Entity);
                }
                StateFrag.BirthTime = World->GetTimeSeconds();

                // Initialize StoredLocation ("home") only if nothing has claimed it yet.
                // UMassActorBindingComponent seeds it from AMassUnitBase::SpawnStoredLocation
                // (a point near the unit's waypoint), and overwriting that with the raw spawn
                // position is what made units walk back to spawn after a fight.
                if (StateFrag.StoredLocation.IsNearlyZero())
                {
                    StateFrag.StoredLocation = TransformList[i].GetTransform().GetLocation();
                }
            }else
            {
                const float Age = World->GetTimeSeconds() - StateFrag.BirthTime;
                const bool bIsOldEnough =  Age >= 1.f;
                
                if (SignalSubsystem && bIsOldEnough)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SyncUnitBase, Entity);
                }
            }

            // --- 1. Check CURRENT entity's health ---
            bool bShouldDie = false;
            if (StatsFrag.MaxHealth > 0.f)
            {
                // Normale Einheiten sterben bei Health <= 0
                bShouldDie = (StatsFrag.Health <= 0.f);
            }
            else if (ImpactFragPtr)
            {
                // EffectAreas mit MaxHealth 0 sterben nur bei expliziter Zerstörungsanforderung
                bShouldDie = ImpactFragPtr->bPendingDestruction;
            }

            if (bShouldDie)
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Dead, Entity);
                }
                continue; // Skip further checks for this dead entity
            }
        } // End Entity Loop
    }); // End ForEachEntityChunk
}

void UMainStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StatsList = ChunkContext.GetMutableFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto ImpactList = ChunkContext.GetFragmentView<FEffectAreaImpactFragment>();
        auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FEffectAreaImpactFragment* ImpactFragPtr = ImpactList.Num() > 0 ? &ImpactList[i] : nullptr;

            HandleLoseSightExtension(StateFrag, StatsFrag);
            
            if (StateFrag.BirthTime == TNumericLimits<float>::Max())
            {
                StateFrag.BirthTime = World->GetTimeSeconds();
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UnitSpawned, Entity);
                }

                // Initialize StoredLocation ("home") only if nothing has claimed it yet.
                // UMassActorBindingComponent seeds it from AMassUnitBase::SpawnStoredLocation
                // (a point near the unit's waypoint), and overwriting that with the raw spawn
                // position is what made units walk back to spawn after a fight.
                if (StateFrag.StoredLocation.IsNearlyZero())
                {
                    StateFrag.StoredLocation = TransformList[i].GetTransform().GetLocation();
                }
            }

            bool bShouldDie = false;
            if (StatsFrag.MaxHealth > 0.f)
            {
                bShouldDie = (StatsFrag.Health <= 0.f);
            }
            else if (ImpactFragPtr)
            {
                bShouldDie = ImpactFragPtr->bPendingDestruction;
            }

            if (bShouldDie)
            {
                HandleUnitDeathClient(Entity, ChunkContext);
            }
        }
    });
}


void UMainStateProcessor::HandleUnitDeathClient(FMassEntityHandle Entity, FMassExecutionContext& Context)
{
    auto& Defer = Context.Defer();
    // Set Dead tag
    Defer.AddTag<FMassStateDeadTag>(Entity);
    // Remove other state tags on client side
    Defer.RemoveTag<FMassStateRunTag>(Entity);
    Defer.RemoveTag<FMassStateChaseTag>(Entity);
    Defer.RemoveTag<FMassStateAttackTag>(Entity);
    Defer.RemoveTag<FMassStatePauseTag>(Entity);
    Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
    Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
    Defer.RemoveTag<FMassStateCastingTag>(Entity);
    Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
    Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
    Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
    Defer.RemoveTag<FMassStateBuildTag>(Entity);
    Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
    Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
    Defer.RemoveTag<FMassStateGoToRepairTag>(Entity);
    Defer.RemoveTag<FMassStateRepairTag>(Entity);
}

void UMainStateProcessor::HandleLoseSightExtension(FMassAIStateFragment& StateFrag,
    FMassCombatStatsFragment& StatsFrag)
{
    if (StateFrag.bHasExtendedLoseSight)
    {
        StateFrag.ExtendedLoseSightTimer -= ExecutionInterval;
        if (StateFrag.ExtendedLoseSightTimer <= 0.f)
        {
            // Clear the additive detection bonus (see ApplyAttackedDetectionBonus in UnitMassTag.h).
            // This used to divide StatsFrag.LoseSightRadius by LoseSightRadiusFaktor instead. That was
            // broken: UUnitActorToFragmentSyncProcessor::SyncCombatStats rewrites LoseSightRadius from
            // the actor every PrePhysics tick, so the *= boost was gone after one tick and this /= then
            // HALVED the configured radius for a tick - the opposite of the intended effect.
            StateFrag.DetectionBonusRadius = 0.f;
            StateFrag.bHasExtendedLoseSight = false;
            StateFrag.ExtendedLoseSightTimer = 0.f;
        }
    }
}
