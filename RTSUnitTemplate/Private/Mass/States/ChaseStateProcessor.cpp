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

            // Haengengebliebenes SwitchingState loesen, bevor die Ausgaenge geprueft
            // werden - sonst steht die Einheit dauerhaft in Chase, ohne sich zu bewegen.
            // Siehe RTSUnitUtils::TickSwitchingStateWatchdog.
            RTSUnitUtils::TickSwitchingStateWatchdog(StateFrag, ExecutionInterval);

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
            const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

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
                }
                else if (const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
                {
                    // Nothing sane to resume to - keep the stop-here behaviour rather than steer to world origin.
                    Pred->Location = TF->GetTransform().GetLocation();
                    Pred->PredDesiredSpeed = 0.f;
                    Pred->bHasData = true;
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
