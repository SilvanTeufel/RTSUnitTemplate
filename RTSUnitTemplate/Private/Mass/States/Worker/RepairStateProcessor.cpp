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
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
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
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::All);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void URepairStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
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

    UWorld* World = EntityManager.GetWorld();
    if (!World) return;

    const bool bIsClient = (World->GetNetMode() == NM_Client);

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, bIsClient](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i);
            }
            else
            {
                ServerExecute(EntityManager, ChunkContext, StateList[i], TargetList[i], StatsList[i], ChunkContext.GetEntity(i), i);
            }
        }
    });
}

void URepairStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& StatsFrag, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    const auto TransformList = Context.GetMutableFragmentView<FTransformFragment>();
    const auto CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
    
    FTransform& CurrentTransform = TransformList[EntityIdx].GetMutableTransform();
    const FMassAgentCharacteristicsFragment& CharFrag = CharList[EntityIdx];

    // 1) Distance regression/out-of-range check with hysteresis
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

    if ((!bIsTargetActive || !bIsTargetAlive) && !StateFrag.SwitchingState)
    {
        UE_LOG(LogTemp, Log, TEXT("[Repair] Target invalid or dead -> GoToBase for Entity [%d:%d]"), Entity.Index, Entity.SerialNumber);
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToBase, Entity);
        }
        return;
    }
    
    if (bIsTargetActive && bIsTargetAlive)
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
        const float ExitBuffer = 40.f;
        const float Dist2D = FVector::Dist2D(CurrentTransform.GetLocation(), FriendlyLoc);

        if (Dist2D > (EffectiveReach + ExitBuffer) && !StateFrag.SwitchingState)
        {
            UE_LOG(LogTemp, Log, TEXT("[Repair] Out of range (%.1f > %.1f) -> GoToRepair for Entity [%d:%d]"), Dist2D, (EffectiveReach + ExitBuffer), Entity.Index, Entity.SerialNumber);
            StateFrag.SwitchingState = true;
            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToRepair, Entity);
            }
            return;
        }

        // Rotation Logic
        FVector LookDir = Dir; // Dir is already normalized FriendlyLoc - CurrentLoc
        if (!LookDir.IsNearlyZero())
        {
            FQuat TargetRotation = FRotationMatrix::MakeFromX(LookDir).ToQuat();
            CurrentTransform.SetRotation(TargetRotation);
        }
    }

    // 2) Cast time reached -> GoToBase
    if (StateFrag.StateTimer >= StatsFrag.CastTime && !StateFrag.SwitchingState)
    {
        UE_LOG(LogTemp, Log, TEXT("[Repair] Cast complete (%.2f/%.2f) -> GoToBase for Entity [%d:%d]"), StateFrag.StateTimer, StatsFrag.CastTime, Entity.Index, Entity.SerialNumber);
        StateFrag.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToBase, Entity);
        }
        return;
    }

    // 3) Drive repair-time synchronization each tick
    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SyncRepairTime, Entity);
}

void URepairStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& AIState, const FMassAITargetFragment& TargetFrag, 
    const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    // Rotation logic on client is now handled by UActorTransformSyncProcessor
}
