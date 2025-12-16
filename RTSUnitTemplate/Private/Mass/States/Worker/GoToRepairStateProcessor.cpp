// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToRepairStateProcessor.h"

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

UGoToRepairStateProcessor::UGoToRepairStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToRepairStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);

    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::All);

    // Exclude unrelated/blocked states
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToRepairStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    if (Owner.GetWorld())
    {
        UE_LOG(LogTemp, Log, TEXT("[GoToRepairStateProcessor] Initialized (NetMode=%d)"), (int32)Owner.GetWorld()->GetNetMode());
    }
}

void UGoToRepairStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

    UWorld* World = EntityManager.GetWorld();

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        UE_LOG(LogTemp, Verbose, TEXT("[GoToRepairStateProcessor] Processing chunk with %d entities"), NumEntities);
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
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

            // Validate friendly target
            const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
            if (!bHasFriendly)
            {
                continue; // Nothing to approach
            }

            // Get friendly target location and radius
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

            // Compute effective repair reach: both capsule radii + base repair distance
            const float BaseRepairDistance = 250.f; // matches AWorkingUnitBase::RepairDistance default
            const float EffectiveReach = CharFrag.CapsuleRadius + FriendlyRadius + BaseRepairDistance;
            const float EnterBuffer = 20.f; // hysteresis buffer for enter

            const float Dist2D = FVector::Dist2D(CurrentTransform.GetLocation(), FriendlyLoc);

            // If within reach, stop and switch to actual Repair state
            if (Dist2D <= (EffectiveReach - EnterBuffer) && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                StopMovement(MoveTarget, World);
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Repair, Entity);
                }
                continue;
            }

            // Otherwise keep chasing the friendly target position
            if (!StateFrag.SwitchingState)
            {
                UpdateMoveTarget(MoveTarget, FriendlyLoc, StatsFrag.RunSpeed, World);
            }
        }
    });
}
