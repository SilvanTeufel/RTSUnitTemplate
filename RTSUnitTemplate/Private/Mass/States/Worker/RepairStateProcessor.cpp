// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/RepairStateProcessor.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

URepairStateProcessor::URepairStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void URepairStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::All);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this);
}

void URepairStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    if (Owner.GetWorld())
    {
        UE_LOG(LogTemp, Log, TEXT("[RepairStateProcessor] Initialized (NetMode=%d)"), (int32)Owner.GetWorld()->GetNetMode());
    }
}

void URepairStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun -= ExecutionInterval;

    if (!SignalSubsystem)
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        UE_LOG(LogTemp, Verbose, TEXT("[RepairStateProcessor] Processing chunk with %d entities"), NumEntities);
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1) Distance regression/out-of-range check with hysteresis
            const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
            if (bHasFriendly)
            {
                FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                float FriendlyRadius = 0.f;
                if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                }
                if (const FMassAgentCharacteristicsFragment* FriendlyChar = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyRadius = FriendlyChar->CapsuleRadius;
                }

                const float BaseRepairDistance = 250.f; // same as AWorkingUnitBase default
                const float EffectiveReach = CharFrag.CapsuleRadius + FriendlyRadius + BaseRepairDistance;
                const float ExitBuffer = 40.f;
                const float Dist2D = FVector::Dist2D(CurrentTransform.GetLocation(), FriendlyLoc);

                if (Dist2D > (EffectiveReach + ExitBuffer) && !StateFrag.SwitchingState)
                {
                    UE_LOG(LogTemp, Log, TEXT("[Repair] Out of range (%.1f > %.1f) -> GoToRepair for Entity [%d:%d]"), Dist2D, (EffectiveReach + ExitBuffer), Entity.Index, Entity.SerialNumber);
                    StateFrag.SwitchingState = true;
                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToRepair, Entity);
                    }
                    continue;
                }
            }

            // 2) Cast time reached -> GoToBase
            if (StateFrag.StateTimer >= StatsFrag.CastTime && !StateFrag.SwitchingState)
            {
                UE_LOG(LogTemp, Log, TEXT("[Repair] Cast complete (%.2f/%.2f) -> GoToBase for Entity [%d:%d]"), StateFrag.StateTimer, StatsFrag.CastTime, Entity.Index, Entity.SerialNumber);
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBase, Entity);
                }
                continue;
            }

            // 3) Drive repair-time synchronization each tick
            SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SyncRepairTime, Entity);
        }
    });
}
