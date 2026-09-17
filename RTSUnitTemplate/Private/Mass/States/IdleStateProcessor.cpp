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
    // Reads fragments of OTHER entities (TargetEntity / FriendlyTargetEntity), which is outside
    // the per-chunk contract of a parallel ForEachEntityChunk. Attack/Chase/Pause/RunState do the
    // same and are game-thread for exactly this reason - keep this one with them. Running it on a
    // worker tripped "Assertion failed: IsEntityValid" inside IsEntityActive itself.
    bRequiresGameThreadExecution = true;
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
        // Mirrors ExecuteServer: needed to keep StoredLocation on the waypoint for
        // travelling units (see the StoredLocation write at the end of the loop).
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        const bool bHasPatrolFrag = PatrolList.Num() > 0;
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

            const FMassPatrolFragment* PatrolFrag = bHasPatrolFrag ? &PatrolList[i] : nullptr;

            // Same order as ExecuteServer: set the home first, so anything below that reads
            // StoredLocation already sees the waypoint and not a stale spawn point.
            if (PatrolFrag && !PatrolFrag->TargetWaypointLocation.IsNearlyZero())
            {
                StateFrag.StoredLocation = GetPatrolHomeLocation(
                    Entity, PatrolFrag->TargetWaypointLocation, PatrolFrag->RandomPatrolRadius, World);
            }

            const bool bPathActive = PathFrag && PathFrag->Waypoints.Num() > PathFrag->CurrentIndex;
            const bool bShouldIgnoreEnemies = bPathActive && !PathFrag->bAttackToggled;
            const bool bIsTargetActive = EntityManager.IsEntityValid(TargetFrag.TargetEntity) && EntityManager.IsEntityActive(TargetFrag.TargetEntity) && EntityManager.IsEntityBuilt(TargetFrag.TargetEntity);

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
                continue;
            }
            
            
            const bool bIsFriendlyActive = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity) && EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
  
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
                        Pred.PredSource = 7; // [PredDiag]
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

            // Generic "walk back to the commanded StoredLocation when bumped" behavior is for regular
            // units. Workers must be EXCLUDED: their idle/next-action is decided by the job system
            // (HandleBaseArea), and StoredLocation is not replicated, so on the client it is stale.
            // For a worker that the server settled near a crowded base (StoredLocation = unreachable
            // base center), this would re-run it toward the center every tick -> the Idle<->Run flip.
            if (!DoesEntityHaveFragment<FMassWorkerStatsFragment>(EntityManager, Entity) &&
                !StateFrag.StoredLocation.IsNearlyZero())
            {
                const float Dist2D = FVector::Dist2D(Transform.GetLocation(), StateFrag.StoredLocation);
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                // Hysterese: die Rueckkehr kostet mehr Abstand als das Ankommen (siehe
                // IdleReturnHysteresis). Ohne sie schiebt die Vermeidung die ruhende Einheit
                // ueber dieselbe Schwelle, die sie zum Zurueck laufen bringt - und wieder.
                const float Threshold = MoveTarget.SlackRadius * TresholdAcceptanceMultiplier * IdleReturnHysteresis;

                if (Dist2D > Threshold)
                {

                    if (bHasPredList)
                    {
                        FMassClientPredictionFragment& Pred = PredictionList[i];
                        Pred.Location = StateFrag.StoredLocation;
                        Pred.PredAcceptanceRadius = MoveTarget.SlackRadius;
                        Pred.PredDesiredSpeed = StatsFrag.RunSpeed;
                        Pred.bHasData = true;
                        Pred.PredSource = 8; // [PredDiag]
                    }
                    if (!StateFrag.SwitchingStateClient)
                    {
                        SwitchToRunState(EntityManager, ChunkContext, Entity, StateFrag);
                    }
                    continue;
                }
            }


            if (bHasPredList)
            {
                FMassClientPredictionFragment& Pred = PredictionList[i];
                const FVector CurrentLocation = Transform.GetLocation();
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                
                Pred.Location = CurrentLocation;
                Pred.PredDesiredSpeed = 0.f;
                
                float AcceptanceRadiusUsed = MoveTarget.SlackRadius;
                if (Pred.PredAcceptanceRadius > KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = Pred.PredAcceptanceRadius;
                }
                else if (AcceptanceRadiusUsed <= KINDA_SMALL_NUMBER)
                {
                    AcceptanceRadiusUsed = 100.f;
                }

                if (FVector::DistSquared2D(CurrentLocation, Pred.Location) <= FMath::Square(AcceptanceRadiusUsed) &&
                    FVector::DistSquared2D(CurrentLocation, MoveTarget.Center) <= FMath::Square(AcceptanceRadiusUsed))
                {
                    Pred.bHasData = false;
                }
            }
            // Units with a waypoint already had their home set at the top of the loop.
            if (!PatrolFrag || PatrolFrag->TargetWaypointLocation.IsNearlyZero())
            {
                StateFrag.StoredLocation = Transform.GetLocation();
            }
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
            const bool bIsTargetActive = EntityManager.IsEntityValid(TargetFrag.TargetEntity) && EntityManager.IsEntityActive(TargetFrag.TargetEntity) && EntityManager.IsEntityBuilt(TargetFrag.TargetEntity);

            if (StateFrag.SwitchingState) continue;

            const bool bIsFriendlyActive = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity) && EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity) && EntityManager.IsEntityBuilt(TargetFrag.FriendlyTargetEntity);
            /*
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
            }*/
            
            if (bIsFriendlyActive)
            {
                // On the server LastKnownFriendlyLocation is only set once (when the follow target is
                // assigned) and is NOT kept up to date while the friendly moves. Read the live transform
                // of the friendly entity so the follow position tracks its current position.
                FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                }
                StateFrag.StoredLocation = RTSUnitUtils::CalculateFollowPosition(EntityManager, Entity, TargetFrag, CharFrag, Transform.GetLocation(), FriendlyLoc, World);
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

            // Refresh the unit's home BEFORE the walk-back branch below reads StoredLocation.
            // Doing it at the end of the loop meant the first idle tick after a fight still
            // saw the stale value (the spawn point), sent the unit all the way back there,
            // and only then corrected to the waypoint - a visible detour.
            if (PatrolFrag && !PatrolFrag->TargetWaypointLocation.IsNearlyZero())
            {
                StateFrag.StoredLocation = GetPatrolHomeLocation(
                    Entity, PatrolFrag->TargetWaypointLocation, PatrolFrag->RandomPatrolRadius, World);
            }

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

            // Workers are excluded from the generic "walk back to StoredLocation" idle behavior (see the
            // matching client-side comment). Otherwise a worker settled near a crowded base (StoredLocation
            // = unreachable base center) would be re-run toward the center every tick, undoing the
            // crowd-settle in GoToBaseStateProcessor and oscillating Idle<->Run.
            if (!DoesEntityHaveFragment<FMassWorkerStatsFragment>(EntityManager, Entity) &&
                !StateFrag.StoredLocation.IsNearlyZero())
            {
                const float Dist2D = FVector::Dist2D(Transform.GetLocation(), StateFrag.StoredLocation);
                const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
                // Hysterese: die Rueckkehr kostet mehr Abstand als das Ankommen (siehe
                // IdleReturnHysteresis). Ohne sie schiebt die Vermeidung die ruhende Einheit
                // ueber dieselbe Schwelle, die sie zum Zurueck laufen bringt - und wieder.
                const float Threshold = MoveTarget.SlackRadius * TresholdAcceptanceMultiplier * IdleReturnHysteresis;

                if (Dist2D > Threshold)
                {
                    UpdateMoveTarget(MoveTargetList[i], StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
                    SwitchToRunState(EntityManager, ChunkContext, Entity, StateFrag);
                    continue;
                }
            }

            // Ensure units stay where they are if target is lost later.
            // Units with a waypoint already had their home set at the top of the loop and
            // must not be re-pinned to wherever a fight happened to end.
            if (!PatrolFrag || PatrolFrag->TargetWaypointLocation.IsNearlyZero())
            {
                StateFrag.StoredLocation = Transform.GetLocation();
            }
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
                Pred->PredSource = 9; // [PredDiag]
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
            Pred->PredSource = 10; // [PredDiag]
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
                Pred->PredSource = 11; // [PredDiag]
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
