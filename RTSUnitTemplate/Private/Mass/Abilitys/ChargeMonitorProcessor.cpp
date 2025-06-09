// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Abilitys/ChargeMonitorProcessor.h"

#include "MassCommonFragments.h"     // For FMassMoveTargetFragment
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h" // For EMassMovementAction
#include "Mass/UnitMassTag.h"

UChargeMonitorProcessor::UChargeMonitorProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All; // Or GameThread as needed
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior; // Or a custom group
    ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks); // Example: Run after movement intent is set
}

void UChargeMonitorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);

    //RegisterQuery(EntityQuery);
    EntityQuery.AddRequirement<FMassChargeTimerFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddTagRequirement<FMassStateChargingTag>(EMassFragmentPresence::All); // MUST have charging tag


    EntityQuery.RegisterWithProcessor(*this);
}


void UChargeMonitorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    EntityQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const TArrayView<FMassChargeTimerFragment> ChargeTimerList = ChunkContext.GetMutableFragmentView<FMassChargeTimerFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
            const TArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetMutableFragmentView<FMassCombatStatsFragment>();
            TArrayView<FMassAIStateFragment> StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

       
            const int32 NumEntities = ChunkContext.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassChargeTimerFragment& ChargeTimer = ChargeTimerList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& State = StateList[i];
            State.StateTimer += ExecutionInterval;
            if (State.StateTimer >= ChargeTimer.ChargeEndTime)
            {
                // 1. Revert speed
                if (ChargeTimer.bOriginalSpeedSet)
                {
                   MoveTarget.DesiredSpeed.Set(ChargeTimer.OriginalDesiredSpeed);
                   StatsList[i].RunSpeed = ChargeTimer.OriginalDesiredSpeed;
                }

                State.StateTimer = 0.f;
                Context.Defer().RemoveFragment<FMassChargeTimerFragment>(Entity);
                Context.Defer().RemoveTag<FMassStateChargingTag>(Entity);
                
            }
        }
    });
}
