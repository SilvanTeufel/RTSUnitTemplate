// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToRepairStateProcessor.h"

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassNavigationFragments.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"

UGoToRepairStateProcessor::UGoToRepairStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
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
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // Client prediction fragment (Optional so server query still matches entities without it).
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

    EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::All);

    // Exclude unrelated/blocked states
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToRepairStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToRepairStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

// CLIENT: prediction only (no tag authoring, no MoveTarget writes). For repair the server keeps
// MoveTarget.Center on the follow ring around the friendly target, so Center is already the stop
// point -> predict toward it directly (arrival distance 0). Center is replicated each update, so
// the client tracks the (moving) target between replications instead of stalling.
void UGoToRepairStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        if (PredList.Num() == 0) return;

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FVector CurrentLocation = TransformList[i].GetTransform().GetLocation();
            const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            PredictWorkerStop(PredList[i], CurrentLocation, MoveTarget.Center, MoveTarget.DesiredSpeed.Get(), 0.f);
        }
    });
}

void UGoToRepairStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    if (!SignalSubsystem)
    {
        return;
    }

    UWorld* World = EntityManager.GetWorld();

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto WorkerStatsList = ChunkContext.GetFragmentView<FMassWorkerStatsFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            if (WorkerStats.BuildingAreaAvailable && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBuild, Entity);
                }
                continue;
            }

            // Validate friendly target
            const FMassEntityHandle TargetEntity = TargetFrag.FriendlyTargetEntity;
            bool bIsTargetActive = EntityManager.IsEntityActive(TargetEntity);
            bool bIsTargetAlive = false;

            if (bIsTargetActive)
            {
                if (const FMassCombatStatsFragment* TargetStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity))
                {
                    bIsTargetAlive = TargetStats->Health > 0.f;
                }
            }

            if (!bIsTargetActive || (!bIsTargetAlive && !StateFrag.SwitchingState))
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::GoToBase, Entity);
                }
                continue;
            }

            // Get friendly target location and radius
            FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
            float FriendlyRadius = 0.f;
            if (const FTransformFragment* FriendlyXform = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity) : nullptr)
            {
                FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
            }
            if (const FMassAgentCharacteristicsFragment* FriendlyChar = bIsTargetActive ? EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.FriendlyTargetEntity) : nullptr)
            {
                FriendlyRadius = FriendlyChar->CapsuleRadius;
            }

            // Compute effective repair reach based on FollowRadius (keep hysteresis)
            const float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
            
            float AttackerRadius = CharFrag.CapsuleRadius;
            float TargetRadius = FriendlyRadius;

            FVector Dir = (FriendlyLoc - CurrentTransform.GetLocation());
            Dir.Z = 0.f;

            if (!Dir.IsNearlyZero())
            {
                Dir.Normalize();
                AttackerRadius = CharFrag.GetRadiusInDirection(Dir, CurrentTransform.GetRotation().Rotator());

                if (const FMassAgentCharacteristicsFragment* FriendlyChar = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    if (FriendlyChar->bUseBoxComponent)
                    {
                        if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                        {
                            TargetRadius = FriendlyChar->GetRadiusInDirection(-Dir, FriendlyXform->GetTransform().GetRotation().Rotator());
                        }
                    }
                }
            }

            const float EffectiveReach = AttackerRadius + TargetRadius + FollowRadius;
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

            // Otherwise keep chasing a position on the FollowRadius ring around the friendly (XY only)
            if (!StateFrag.SwitchingState)
            {
                FVector ToSelf2D = (CurrentTransform.GetLocation() - FriendlyLoc);
                ToSelf2D.Z = 0.f;
                const float Len = ToSelf2D.Size2D();
                FVector Dir2D = Len > KINDA_SMALL_NUMBER ? ToSelf2D / Len : FVector::XAxisVector;
                FVector DesiredPos = FriendlyLoc + Dir2D * FollowRadius;

                if (const FMassAgentCharacteristicsFragment* FriendlyChar = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    DesiredPos.Z = FriendlyChar->LastGroundLocation;
                }
                else
                {
                    DesiredPos.Z = FriendlyLoc.Z; // ignore Z for follow
                }
                
                UpdateMoveTarget(MoveTarget, DesiredPos, StatsFrag.RunSpeed, World);
            }
        }
    });
}
