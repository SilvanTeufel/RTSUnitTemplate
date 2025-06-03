// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Abilitys/ChargeMonitorProcessor.h"

#include "MassCommonFragments.h"     // For FMassMoveTargetFragment
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h" // For EMassMovementAction
#include "Mass/UnitMassTag.h"

UChargeMonitorProcessor::UChargeMonitorProcessor()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All; // Or GameThread as needed
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior; // Or a custom group
    ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks); // Example: Run after movement intent is set
}

void UChargeMonitorProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FMassChargeTimerFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddTagRequirement<FMassStateChargingTag>(EMassFragmentPresence::All); // MUST have charging tag
    EntityQuery.RegisterWithProcessor(*this);
}

void UChargeMonitorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const TArrayView<FMassChargeTimerFragment> ChargeTimerList = ChunkContext.GetMutableFragmentView<FMassChargeTimerFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
            const TArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetMutableFragmentView<FMassCombatStatsFragment>();

       
            const int32 NumEntities = ChunkContext.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassChargeTimerFragment& ChargeTimer = ChargeTimerList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            if (CurrentTime >= ChargeTimer.ChargeEndTime)
            {
                UE_LOG(LogTemp, Log, TEXT("ChargeMonitor: Entity %s charge duration ended."), *Entity.DebugGetDescription());

                // 1. Revert speed
                if (ChargeTimer.bOriginalSpeedSet)
                {
                    MoveTarget.DesiredSpeed.Set(ChargeTimer.OriginalDesiredSpeed);
                }
                else
                {
                    // Fallback if original speed wasn't set (e.g., to a default idle speed)
                    MoveTarget.DesiredSpeed.Set(StatsList[i].RunSpeed); // Or some base speed
                }
                // 2. Remove charging tag and FMassChargeTimerFragment
                // Defer fragment removal to avoid issues while iterating if not careful,
                // or simply let it be (it won't be processed next time without the tag).
                // For cleanliness, deferred removal is better.
                Context.Defer().RemoveFragment<FMassChargeTimerFragment>(Entity);
                Context.Defer().RemoveTag<FMassStateChargingTag>(Entity);


                // 3. (Optional) Add a new state tag, e.g., Idle
                // ChunkContext.AddTag<FMassStateIdleTag>(Entity); // Ensure FMassStateIdleTag is defined

                // Reset the flag in the fragment if you are not removing it (though removal is cleaner)
                // ChargeTimer.bOriginalSpeedSet = false;
            }
        }
    });
}