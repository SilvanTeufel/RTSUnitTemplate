// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/RunStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Core/RTSUnitUtils.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"
#include "Async/Async.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"

URunStateProcessor::URunStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void URunStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
 EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All); // Tag optional auf Client prüfen
    
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);       // Aktuelle Position lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel-Daten lesen, Stoppen erfordert Schreiben
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
   	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

void URunStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    // On client, only check for arrival and switch to Idle locally by adjusting tags via deferred commands.
    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        const bool bHasPredList = PredictionList.Num() > 0;
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];

            // === BatchDiag (TEMP): commanded unit being processed in Run. Watch Det flag + whether it re-engages below. ===
            RTS_BatchDiagLog(TEXT("RUN-CLIENT"), World, EntityManager, Entity,
                Cast<AUnitBase>(ActorList[i].Get()) ? Cast<AUnitBase>(ActorList[i].Get())->UnitIndex : -1,
                bHasPredList ? &PredictionList[i] : nullptr);

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
            }

            // If already dead, do not modify any tags on client
            if (DoesEntityHaveTag(EntityManager, Entity, FMassStateDeadTag::StaticStruct()))
            {
                continue;
            }
            // If health is zero or below, mark as dead and skip further processing
            if (Stats.Health <= 0.f)
            {
                auto& Defer = ChunkContext.Defer();
                Defer.AddTag<FMassStateDeadTag>(Entity);
                continue;
            }

            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            
            if (StateFrag.HoldPosition)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    StateFrag.StoredLocation = CurrentLocation;

                    if (bHasPredList)
                    {
                        FMassClientPredictionFragment& Pred = PredictionList[i];
                        Pred.Location = StateFrag.StoredLocation;
                        Pred.PredDesiredSpeed = Stats.RunSpeed;
                        Pred.PredAcceptanceRadius = 100.f;
                        Pred.bHasData = true;
                    }
                    
                    SwitchToIdleState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }
            
            // A unit with a FRESH client prediction (bHasData) must measure "arrival" against its COMMANDED target
            // (Pred.Location), NOT the replicated MoveTarget.Center. The latter can be stale (the new target hasn't
            // round-tripped through the server yet — and with replication off it NEVER updates), so the unit standing
            // near the OLD MoveTarget.Center is falsely flagged "arrived" right after a command -> SwitchToIdleState
            // resets Pred.Location to the current position -> the mover's convergence check clears bHasData -> the
            // prediction is destroyed before it can drive movement. Using Pred.Location breaks that cycle.
            const bool bClientPredicting = bHasPredList && PredictionList[i].bHasData;
            FVector FinalDestination = bClientPredicting ? PredictionList[i].Location : MoveTarget.Center;
            float AcceptanceRadius = bClientPredicting
                ? FMath::Max(PredictionList[i].PredAcceptanceRadius, MoveTarget.SlackRadius)
                : MoveTarget.SlackRadius;

            // Only arrival check on client (skip if following a unit)
            const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
         
            if (FVector::Dist2D(CurrentLocation, FinalDestination) <= AcceptanceRadius || 
                (FVector::Dist2D(CurrentLocation, FinalDestination) <= VelocityDistanceCheck && VelocityList[i].Value.IsNearlyZero(VelocityToIdle)))
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    VelocityList[i].Value = FVector::ZeroVector;
                    SwitchToIdleState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                }
                continue;
            }

            const bool bHasDetectTag = DoesEntityHaveTag(EntityManager, Entity, FMassStateDetectTag::StaticStruct());

            if (bHasDetectTag && !bHasFriendly)
            {
                const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
                if (TargetFrag.bHasValidTarget && bIsTargetActive)
                {
                    if (!Stats.bCanMoveWhileAttacking)
                    {
                        if (!StateFrag.SwitchingStateClient)
                        {
                            // === BatchDiag (TEMP): RUN re-engages a commanded unit back into Chase (Detect + valid target survived). ===
                            RTS_BatchDiagLog(TEXT("RUN-REENGAGE->Chase"), World, EntityManager, Entity,
                                Cast<AUnitBase>(ActorList[i].Get()) ? Cast<AUnitBase>(ActorList[i].Get())->UnitIndex : -1,
                                bHasPredList ? &PredictionList[i] : nullptr);
                            SwitchToChaseState(EntityManager, ChunkContext, Entity, StateFrag);
                        }
                        continue;
                    }
                    else
                    {
                        // For bCanMoveWhileAttacking units, we check if they should switch to Pause (start attacking)
                        const float DistSq = FVector::DistSquared2D(CurrentLocation, TargetFrag.LastKnownLocation);

                        const FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
                        const bool bIsEnemyActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
                        const FMassAgentCharacteristicsFragment* TargetCharFrag = bIsEnemyActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
                        const FTransformFragment* TargetTransformFrag = bIsEnemyActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
                        const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

                        const float CombinedRadii = CharFrag ? RTSUnitUtils::GetCombinedRadii(*CharFrag, CurrentMassTransform, TargetCharFrag, TargetTransform, TargetFrag.LastKnownLocation) : 0.f;
                        const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;
                        const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                        if (DistSq <= AttackRangeSq)
                        {
                            if (!StateFrag.SwitchingStateClient)
                            {
                                SwitchToPauseState(EntityManager, ChunkContext, Entity, StateFrag);
                            }
                            continue;
                        }
                    }
                }
            }
        }
    });
}

void URunStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    if (!SignalSubsystem) return;
    
    EntityQuery.ForEachEntityChunk(Context,

        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            if (!DoesEntityHaveTag(EntityManager, Entity, FMassStateRunTag::StaticStruct()))
            {
                continue;
            }

            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for Update/Stop
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();

            // Recalculate follow position if moving and following
          const bool bIsFriendlyActiveServer = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
            /*
            if (bIsFriendlyActiveServer)
            {
                FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                }

                FVector DesiredPos = RTSUnitUtils::CalculateFollowPosition(EntityManager, Entity, TargetFrag, &CharFrag, CurrentLocation, FriendlyLoc, World);
                UpdateMoveTarget(MoveTarget, DesiredPos, Stats.RunSpeed, World);
            }
            */ 
            if (StateFrag.HoldPosition)
            {
                StateFrag.StoredLocation = CurrentLocation;
                StopMovement(MoveTargetList[i], World);
                SwitchToIdleState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                continue;
            }
            
            const FVector FinalDestination = MoveTarget.Center;
            const float AcceptanceRadius = MoveTarget.SlackRadius; //150.f;

            StateFrag.StateTimer += ExecutionInterval;

            const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
            
            if (DoesEntityHaveTag(EntityManager,Entity, FMassStateDetectTag::StaticStruct()) && TargetFrag.bHasValidTarget && bIsTargetActive && !Stats.bCanMoveWhileAttacking && !bIsFriendlyActiveServer)
            {
                SwitchToChaseState(EntityManager, ChunkContext, Entity, StateFrag);
                continue;
            }
            else if (FVector::Dist2D(CurrentLocation, FinalDestination) <= AcceptanceRadius || 
                     (FVector::Dist2D(CurrentLocation, FinalDestination) <= VelocityDistanceCheck && VelocityList[i].Value.IsNearlyZero(VelocityToIdle)))
            {
                VelocityList[i].Value = FVector::ZeroVector;
                SwitchToIdleState(EntityManager, ChunkContext, Entity, StateFrag, ActorList[i].GetMutable());
                continue;
            }
            else if ( DoesEntityHaveTag(EntityManager, Entity, FMassStateDetectTag::StaticStruct()) &&
                    TargetFrag.bHasValidTarget && bIsTargetActive && Stats.bCanMoveWhileAttacking && !bIsFriendlyActiveServer)
            {
                    const float DistSq = FVector::DistSquared2D(CurrentLocation, TargetFrag.LastKnownLocation);
                    
                    const FMassAgentCharacteristicsFragment* TargetCharFragPtr = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity) : nullptr;
                    const FTransformFragment* TargetTransformFrag = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.TargetEntity) : nullptr;
                    const FTransform* TargetTransform = TargetTransformFrag ? &TargetTransformFrag->GetTransform() : nullptr;

                    const float CombinedRadii = RTSUnitUtils::GetCombinedRadii(CharFrag, CurrentMassTransform, TargetCharFragPtr, TargetTransform, TargetFrag.LastKnownLocation);
                    const float EffectiveAttackRange = Stats.AttackRange + CombinedRadii;
                    const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                    if (DistSq <= AttackRangeSq && !StateFrag.SwitchingState)
                    {
                        SwitchToPauseState(EntityManager, ChunkContext, Entity, StateFrag);
                        continue;
                    }
            }
            
        }
    }); // End ForEachEntityChunk

}

void URunStateProcessor::SwitchToIdleState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag, AActor* UnitActor)
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
        if (FMassClientPredictionFragment* Pred = EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
        {
            if (const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
            {
                Pred->Location = TransformFrag->GetTransform().GetLocation();
            }
            Pred->PredDesiredSpeed = 0.f;
            Pred->bHasData = true;
        }

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

void URunStateProcessor::SwitchToChaseState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
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
            if (const FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity))
            {
                Pred->Location = TargetFrag->LastKnownLocation;
                if (const FMassCombatStatsFragment* Stats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
                {
                    Pred->PredDesiredSpeed = Stats->RunSpeed;
                }
                Pred->bHasData = true;
            }
        }
        
        StateFrag.SwitchingStateClient = true;
        Defer.RemoveTag<FMassStateRunTag>(Entity);
        Defer.RemoveTag<FMassStateIdleTag>(Entity);
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
        Defer.AddTag<FMassStateChaseTag>(Entity);
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Chase, Entity);
        }
    }
}

void URunStateProcessor::SwitchToPauseState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        const FMassCombatStatsFragment* Stats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);
        if (FMassClientPredictionFragment* Pred = EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
        {
            if (!Stats || !Stats->bCanMoveWhileAttacking)
            {
                if (const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
                {
                    Pred->Location = TransformFrag->GetTransform().GetLocation();
                }
                Pred->PredDesiredSpeed = 0.f;
                Pred->bHasData = true;
            }
        }
        
        auto& Defer = Context.Defer();
        StateFrag.SwitchingStateClient = true;
        Defer.RemoveTag<FMassStateRunTag>(Entity);
        Defer.RemoveTag<FMassStateIdleTag>(Entity);
        Defer.RemoveTag<FMassStateChaseTag>(Entity);
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
        Defer.RemoveTag<FMassStateCastingTag>(Entity);
        Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
        Defer.RemoveTag<FMassStateBuildTag>(Entity);
        Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
        Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
        Defer.AddTag<FMassStatePauseTag>(Entity);
    }
    else
    {
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Pause, Entity);
        }
    }
}
