// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/PatrolRandomStateProcessor.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"

#include "MassActorSubsystem.h"   
#include "MassNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h" // Für Cast
#include "Actors/Waypoint.h"      // Für Waypoint-Interaktion (falls noch nötig)
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

UPatrolRandomStateProcessor::UPatrolRandomStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UPatrolRandomStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
	EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::All); // Nur Chase-Entitäten

	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)
    
	EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadWrite); // Request the patrol fragment

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateNeedsInitialKickTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UPatrolRandomStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UPatrolRandomStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling Check ---
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
       return;
    }

    TimeSinceLastRun -= ExecutionInterval;


    UWorld* World = Context.GetWorld();
    if (!World) return;

    if (!SignalSubsystem) return;
    

    EntityQuery.ForEachEntityChunk(Context,

        [this, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer reset
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for StopMovement
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer reset
            
            const float Age = World->GetTimeSeconds() - StateFrag.BirthTime;
            const bool bIsOldEnough =  Age >= 1.f;
         
            
       

            const FMassAITargetFragment& TargetFrag = TargetList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for StopMovement
            const FTransformFragment& TransformFrag = TransformList[i];

            // --- 1. Check for sighted enemy ---
            if (TargetFrag.bHasValidTarget && !StateFrag.SwitchingState)
            {
                // Queue signal instead of sending directly
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                }
                continue; // Switch state, process next entity
            }

            
                const FVector CurrentLocation = TransformFrag.GetTransform().GetLocation();
                const FVector CurrentDestination = MoveTarget.Center;
                const float AcceptanceRadius = MoveTarget.SlackRadius * 4;
                const float Dist= FVector::Dist2D(CurrentLocation, CurrentDestination);
                
                
                if (Dist <= AcceptanceRadius && !StateFrag.SwitchingState && bIsOldEnough)
                {
                    const FMassPatrolFragment& PatrolFrag = PatrolList[i];
                    const bool bHasWaypoint = !PatrolFrag.TargetWaypointLocation.IsNearlyZero();

                    // StoredLocation is the unit's "home": every return-to-post path
                    // (ChaseStateProcessor's chase-abort, IdleStateProcessor's walk-back)
                    // moves the unit there. Storing the position it happened to reach made
                    // a unit that is still TRAVELLING to its waypoint turn around after a
                    // fight. Its home is the waypoint, so store that instead.
                    StateFrag.StoredLocation = bHasWaypoint
                        ? GetPatrolHomeLocation(Entity, PatrolFrag.TargetWaypointLocation, PatrolFrag.RandomPatrolRadius)
                        : CurrentLocation;

                    // Idling between patrol legs is only allowed near the waypoint
                    // (RandomPatrolRadius == mean of AWaypoint::PatrolCloseOffset). Arriving
                    // at some intermediate point - e.g. a stale move target left over from a
                    // chase - must not burn the PatrolCloseIdlePercentage roll far from the
                    // goal, so re-target and keep going instead.
                    if (bHasWaypoint &&
                        FVector::Dist2D(CurrentLocation, PatrolFrag.TargetWaypointLocation)
                            > PatrolFrag.RandomPatrolRadius + AcceptanceRadius)
                    {
                        // PISwitcher -> UUnitStateProcessor::IdlePatrolSwitcher is what actually
                        // picks a NEW patrol target. UnitSignals::PatrolRandom would only swap
                        // the state tags and leave the unit sitting on the reached target.
                        StateFrag.SwitchingState = true;
                        if (SignalSubsystem)
                        {
                            SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::PISwitcher, Entity);
                        }
                        StateFrag.StateTimer = 0.f;
                        continue;
                    }

                    StateFrag.SwitchingState = true;

                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::PatrolIdle, Entity);
                    }
                    StateFrag.StateTimer = 0.f;
                    StopMovement(MoveTarget, World);

                    continue;
                }
                

        } // End Entity Loop
    }); // End ForEachEntityChunk


}
