// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/IdleStateProcessor.h" // Dein Prozessor-Header

// Andere notwendige Includes...
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"      // FMassVelocityFragment
#include "MassNavigationFragments.h"
#include "MassCommonFragments.h"
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"

// ...

UIdleStateProcessor::UIdleStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UIdleStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);


    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); 
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassUnitPathFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UIdleStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UIdleStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    const bool bIsClient = Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client);
    
    if (bIsClient)
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UIdleStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    EntityQuery.ForEachEntityChunk(Context, [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto CharacteristicsList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const bool bHasCharFrag = CharacteristicsList.Num() > 0;
        const auto PathList = ChunkContext.GetFragmentView<FMassUnitPathFragment>();
        const bool bHasPathFrag = PathList.Num() > 0;
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        const bool bHasPredList = PredictionList.Num() > 0;
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassAgentCharacteristicsFragment* CharFrag = bHasCharFrag ? &CharacteristicsList[i] : nullptr;
            const FMassUnitPathFragment* PathFrag = bHasPathFrag ? &PathList[i] : nullptr;

            const bool bPathActive = PathFrag && PathFrag->Waypoints.Num() > PathFrag->CurrentIndex;
            const bool bShouldIgnoreEnemies = bPathActive && !PathFrag->bAttackToggled;
            const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
                continue;
            }
            
            
            const bool bIsFriendlyActive = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
  
            if (bIsFriendlyActive && !StateFrag.HoldPosition)
            {
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                                
                FVector DesiredPos = MoveTarget.Center;
                StateFrag.StoredLocation = DesiredPos;

                const float Dist2D = FVector::Dist2D(Transform.GetLocation(), DesiredPos);
                              
                const float Threshold = MoveTarget.SlackRadius * FollowAcceptanceMultiplier;

                if (Dist2D > Threshold)
                {
                    if (bHasPredList)
                    {
                        FMassClientPredictionFragment& Pred = PredictionList[i];
                        Pred.Location = DesiredPos;
                        Pred.PredAcceptanceRadius = MoveTarget.SlackRadius;
                        Pred.PredDesiredSpeed = StatsFrag.RunSpeed;
                        Pred.bHasData = true;
                    }
                    SwitchToRunState(EntityManager, ChunkContext, Entity, StateFrag);
                    continue;
                }
            }
            
            
            if (TargetFrag.bHasValidTarget && bIsTargetActive && !StateFrag.HoldPosition && !bShouldIgnoreEnemies && !bIsFriendlyActive)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    SwitchToChaseState(EntityManager, ChunkContext, Entity, StateFrag);
                }
                continue;
            }

            if (TargetFrag.bHasValidTarget && bIsTargetActive && (StateFrag.HoldPosition || bIsFriendlyActive) && !bShouldIgnoreEnemies)
            {
                const float EffectiveAttackRange = StatsFrag.AttackRange;
                const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
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

            if (!StateFrag.StoredLocation.IsNearlyZero())
            {
                const float Dist2D = FVector::Dist2D(Transform.GetLocation(), StateFrag.StoredLocation);
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                const float Threshold = MoveTarget.SlackRadius * TresholdAcceptanceMultiplier;
                
                if (Dist2D > Threshold)
                {
                    
                    if (bHasPredList)
                    {
                        FMassClientPredictionFragment& Pred = PredictionList[i];
                        Pred.Location = StateFrag.StoredLocation;
                        Pred.PredAcceptanceRadius = MoveTarget.SlackRadius;
                        Pred.PredDesiredSpeed = StatsFrag.RunSpeed;
                        Pred.bHasData = true;
                    }
                    if (!StateFrag.SwitchingStateClient)
                    {
                        SwitchToRunState(EntityManager, ChunkContext, Entity, StateFrag);
                    }
                    continue;
                }
            }

            // --- NEU: Prediction für Idle-Einheiten sicherstellen ---
            // Wenn wir hier ankommen, bleibt die Einheit im Idle-Zustand.
            // Wir setzen die Prediction auf Stop, um lokales Drift zu verhindern.
            if (bHasPredList)
            {
                FMassClientPredictionFragment& Pred = PredictionList[i];
                Pred.Location = Transform.GetLocation();
                Pred.PredDesiredSpeed = 0.f;
                Pred.bHasData = true;
            }
            StateFrag.StoredLocation = Transform.GetLocation();
        }
    });
}

void UIdleStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World || !SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context, [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto CharacteristicsList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const bool bHasCharFrag = CharacteristicsList.Num() > 0;
        const auto PathList = ChunkContext.GetFragmentView<FMassUnitPathFragment>();
        const bool bHasPathFrag = PathList.Num() > 0;
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        const bool bHasPatrolFrag = PatrolList.Num() > 0;
        auto PredictionList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        const bool bHasPredList = PredictionList.Num() > 0;

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassPatrolFragment* PatrolFrag = bHasPatrolFrag ? &PatrolList[i] : nullptr;
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassAgentCharacteristicsFragment* CharFrag = bHasCharFrag ? &CharacteristicsList[i] : nullptr;
            const FMassUnitPathFragment* PathFrag = bHasPathFrag ? &PathList[i] : nullptr;

            const bool bPathActive = PathFrag && PathFrag->Waypoints.Num() > PathFrag->CurrentIndex;
            const bool bShouldIgnoreEnemies = bPathActive && !PathFrag->bAttackToggled;
            const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);

            if (StateFrag.SwitchingState) continue;

            const bool bIsFriendlyActive = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);

            if (bIsFriendlyActive && !StateFrag.HoldPosition)
            {
                FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                }

                FVector DesiredPos = RTSUnitUtils::CalculateFollowPosition(EntityManager, Entity, TargetFrag, CharFrag, Transform.GetLocation(), FriendlyLoc, World);
                StateFrag.StoredLocation = DesiredPos;

                const float Dist2D = FVector::Dist2D(Transform.GetLocation(), DesiredPos);
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                const float Threshold = MoveTarget.SlackRadius * FollowAcceptanceMultiplier;
                if (Dist2D > Threshold)
                {
                    UpdateMoveTarget(MoveTargetList[i], DesiredPos, StatsFrag.RunSpeed, World);
                    SwitchToRunState(EntityManager, ChunkContext, Entity, StateFrag);
                    continue;
                }
            }
            
            if (TargetFrag.bHasValidTarget && bIsTargetActive && !StateFrag.HoldPosition && !bShouldIgnoreEnemies && !bIsFriendlyActive)
            {
                SwitchToChaseState(EntityManager, ChunkContext, Entity, StateFrag);
                continue;
            }

            if (TargetFrag.bHasValidTarget && bIsTargetActive && (StateFrag.HoldPosition || bIsFriendlyActive))
            {
                const float EffectiveAttackRange = StatsFrag.AttackRange;
                const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    SwitchToPauseState(EntityManager, ChunkContext, Entity, StateFrag);
                    continue;
                }
            }
            

            StateFrag.StateTimer += ExecutionInterval;
            
            if (PatrolFrag)
            {
                bool bHasPatrolRoute = PatrolFrag->CurrentWaypointIndex != INDEX_NONE;
                bool bIsOnPlattform = false;
            
                if (!bIsOnPlattform && PatrolFrag->bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= PatrolFrag->SetUnitsBackToPatrolTime)
                {
                    SwitchToPatrolRandomState(EntityManager, ChunkContext, Entity, StateFrag);
                    continue;
                }
            }

            if (!StateFrag.StoredLocation.IsNearlyZero())
            {
                const float Dist2D = FVector::Dist2D(Transform.GetLocation(), StateFrag.StoredLocation);
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                const float Threshold = MoveTarget.SlackRadius * TresholdAcceptanceMultiplier;

                if (Dist2D > Threshold)
                {
                    UpdateMoveTarget(MoveTargetList[i], StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
                    SwitchToRunState(EntityManager, ChunkContext, Entity, StateFrag);
                    continue;
                }
            }

            // Ensure units stay where they are if target is lost later
            StateFrag.StoredLocation = Transform.GetLocation();
        }
    });
}


void UIdleStateProcessor::SwitchToChaseState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
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
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Chase, Entity);
        }
    }
}

void UIdleStateProcessor::SwitchToPauseState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
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
        auto& Defer = Context.Defer();
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
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Pause, Entity);
        }
    }
}

void UIdleStateProcessor::SwitchToRunState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        
        if (FMassClientPredictionFragment* Pred = EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(Entity))
        {
            //if (const FMassMoveTargetFragment* MoveTarget = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
            if (!Pred->bHasData)
            {
                //Pred->Location = MoveTarget->Center;
                if (const FMassCombatStatsFragment* Stats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
                {
                    Pred->PredDesiredSpeed = Stats->RunSpeed;
                }
                Pred->bHasData = true;
            }
        }
        
        StateFrag.SwitchingStateClient = true;
        auto& Defer = Context.Defer();
        Defer.RemoveTag<FMassStateIdleTag>(Entity);
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
        Defer.AddTag<FMassStateRunTag>(Entity);
    }
    else
    {
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Run, Entity);
        }
    }
}

void UIdleStateProcessor::SwitchToPatrolRandomState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
        StateFrag.SwitchingStateClient = true;
        auto& Defer = Context.Defer();
        Defer.RemoveTag<FMassStateIdleTag>(Entity);
        Defer.RemoveTag<FMassStateRunTag>(Entity);
        Defer.RemoveTag<FMassStateChaseTag>(Entity);
        Defer.RemoveTag<FMassStateAttackTag>(Entity);
        Defer.RemoveTag<FMassStatePauseTag>(Entity);
        Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
        Defer.RemoveTag<FMassStateCastingTag>(Entity);
        Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
        Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
        Defer.RemoveTag<FMassStateBuildTag>(Entity);
        Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
        Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);
        Defer.AddTag<FMassStatePatrolRandomTag>(Entity);
    }
    else
    {
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::PatrolRandom, Entity);
        }
    }
}
