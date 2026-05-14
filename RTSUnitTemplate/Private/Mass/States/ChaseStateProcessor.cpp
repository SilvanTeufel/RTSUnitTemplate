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
#include "Controller\PlayerController\CustomControllerBase.h"


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

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

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
    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const auto NetIDList = ChunkContext.GetFragmentView<FMassNetworkIDFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
            }

            // If already dead, skip any client-side tag manipulation
            if (DoesEntityHaveTag(EntityManager, Entity, FMassStateDeadTag::StaticStruct()))
            {
                continue;
            }

            // If health zero or below, mark dead and skip further changes
            if (Stats.Health <= 0.f)
            {
                UE_LOG(LogTemp, Log, TEXT("[ChaseClient] Entity[%d:%d] Health=%.2f -> Dead"), Entity.Index, Entity.SerialNumber, Stats.Health);
                auto& Defer = ChunkContext.Defer();
                Defer.AddTag<FMassStateDeadTag>(Entity);
                continue;
            }

            // Case 1: Hold position => switch to Idle locally
            if (StateFrag.HoldPosition)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    UE_LOG(LogTemp, Log, TEXT("[ChaseClient] Entity[%d:%d] HoldPosition -> SwitchToIdle"), Entity.Index, Entity.SerialNumber);
                    SwitchToIdleState(ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }

            // Case 2: Lost/invalid target AND placeholder is Idle => switch to Idle locally
            if (!EntityManager.IsEntityActive(TargetFrag.TargetEntity) || (!TargetFrag.bHasValidTarget))
            {
                if (StateFrag.PlaceholderSignal == UnitSignals::Idle)
                {
                    if (!StateFrag.SwitchingStateClient)
                    {
                        UE_LOG(LogTemp, Log, TEXT("[ChaseClient] Entity[%d:%d] Target Invalid & Idle Placeholder -> SwitchToIdle"), Entity.Index, Entity.SerialNumber);
                        SwitchToIdleState(ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                    }
                    continue;
                }
            }

            // Case 3: In range check for local state switch to Pause
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];

            bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);

            if (bIsTargetActive)
            {
                const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

                const FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity);
                const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity);
                const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

                const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, Transform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation);
                const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                UE_LOG(LogTemp, Log, TEXT("[ChaseClient] Entity[%d:%d] NetID[%u] DistSq=%.2f AttackRangeSq=%.2f (Range=%.2f, Radii=%.2f)"), 
                    Entity.Index, Entity.SerialNumber, (uint32)NetIDList[i].NetID.GetValue(), DistSq, AttackRangeSq, Stats.AttackRange, CombinedRadii);
                
                if (DistSq <= AttackRangeSq)
                {
                    if (!StateFrag.SwitchingStateClient)
                    {
                        UE_LOG(LogTemp, Log, TEXT("[ChaseClient] Entity[%d:%d] In Range -> SwitchToPause"), Entity.Index, Entity.SerialNumber);
                        StateFrag.SwitchingStateClient = true;
                        StateFrag.StateTimerClient = 0.f;

                        // Tags lokal manipulieren, damit der PauseStateProcessor übernimmt
                        auto& Defer = ChunkContext.Defer();
                        Defer.RemoveTag<FMassStateChaseTag>(Entity);
                        Defer.AddTag<FMassStatePauseTag>(Entity);

                        UWorld* World = ChunkContext.GetWorld();
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
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("[ChaseClient] Entity[%d:%d] Target[%d:%d] NOT ACTIVE (bHasValidTarget=%d)"), 
                    Entity.Index, Entity.SerialNumber, TargetFrag.TargetEntity.Index, TargetFrag.TargetEntity.SerialNumber, TargetFrag.bHasValidTarget);
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
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                }
                continue;
            }
            
            bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
            if (!bIsTargetActive || (!TargetFrag.bHasValidTarget && !StateFrag.SwitchingState))
            {
                // Queue signal instead of sending directly
                if (bHasMoveTarget)
                {
                    UpdateMoveTarget(
                     MoveTargetList[i],
                     StateFrag.StoredLocation,
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

            // --- Distance Check ---
    
            const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

            const FMassAgentCharacteristicsFragment* TargetCharFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
            const FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
            const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

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

           const FMassAgentCharacteristicsFragment* TargetCharFragPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
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

void UChaseStateProcessor::SwitchToIdleState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag, AActor* UnitActor)
{
    auto& Defer = Context.Defer();

    if (StateFrag.CanAttack && StateFrag.IsInitialized)
    {
        Defer.AddTag<FMassStateDetectTag>(Entity);
    }
    
    StateFrag.PlaceholderSignal = UnitSignals::Idle;
    if (AUnitBase* UnitBase = Cast<AUnitBase>(UnitActor))
    {
        UnitBase->UnitStatePlaceholder = UnitData::Idle;
    }
    
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        StateFrag.SwitchingStateClient = true;
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
        Defer.AddTag<FMassStateIdleTag>(Entity);
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Idle, Entity);
        }
    }
}

// void UChaseStateProcessor::CalculateRadii...
