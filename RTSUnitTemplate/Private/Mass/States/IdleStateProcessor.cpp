// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/IdleStateProcessor.h" // Dein Prozessor-Header

// Andere notwendige Includes...
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"      // FMassVelocityFragment
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

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
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); 

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassUnitPathFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
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

    // Throttle follow assignment checks to once per second
    FollowAccum += ExecutionInterval;
    this->bFollowTickThisFrame = (FollowAccum >= 1.0f);
    if (this->bFollowTickThisFrame)
    {
        FollowAccum = 0.0f;
    }
    
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
    EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto PathList = ChunkContext.GetFragmentView<FMassUnitPathFragment>();
        const bool bHasPathFrag = PathList.Num() > 0;

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassUnitPathFragment* PathFrag = bHasPathFrag ? &PathList[i] : nullptr;

            if (StateFrag.SwitchingStateClient)
            {
                StateFrag.SwitchingStateClient = false;
            }

            const bool bPathActive = PathFrag && PathFrag->Waypoints.Num() > PathFrag->CurrentIndex;
            const bool bShouldIgnoreEnemies = bPathActive && !PathFrag->bAttackToggled;
            const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);
            
            if (TargetFrag.bHasValidTarget && bIsTargetActive && !StateFrag.HoldPosition && !bShouldIgnoreEnemies)
            {
                if (!StateFrag.SwitchingStateClient)
                {
                    SwitchToChaseState(ChunkContext, Entity, StateFrag);
                }
                continue;
            }

            if (TargetFrag.bHasValidTarget && bIsTargetActive && StateFrag.HoldPosition)
            {
                const float EffectiveAttackRange = StatsFrag.AttackRange;
                const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    if (!StateFrag.SwitchingStateClient)
                    {
                        SwitchToPauseState(ChunkContext, Entity, StateFrag);
                    }
                    continue;
                }
            }
            
            if (this->bFollowTickThisFrame)
            {
                const bool bIsFriendlyActive = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
                if (bIsFriendlyActive)
                {
                    FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                    if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                    {
                        FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                    }

                    const float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
                    FVector ToSelf2D = (Transform.GetLocation() - FriendlyLoc);
                    ToSelf2D.Z = 0.f;
                    const float Len2D = ToSelf2D.Size2D();
                    const FVector Dir2D = (Len2D > KINDA_SMALL_NUMBER) ? (ToSelf2D / Len2D) : FVector::XAxisVector;
                    FVector DesiredPos = FriendlyLoc + Dir2D * FollowRadius;

                    float OffsetMag = FMath::Clamp(TargetFrag.FollowOffset, 0.f, FollowRadius);
                    if (OffsetMag > 0.f)
                    {
                        uint64 Seed = (uint64)Entity.Index | ((uint64)Entity.SerialNumber << 32);
                        Seed += 0x9E3779B97F4A7C15ull;
                        Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
                        Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
                        Seed ^= (Seed >> 31);
                        const double Unit = (double)(Seed >> 11) * (1.0 / 9007199254740992.0);
                        const float Angle = (float)(Unit * 2.0 * PI);
                        const float CosA = FMath::Cos(Angle);
                        const float SinA = FMath::Sin(Angle);
                        DesiredPos.X += CosA * OffsetMag;
                        DesiredPos.Y += SinA * OffsetMag;
                    }

                    const float Dist2D = FVector::Dist2D(Transform.GetLocation(), DesiredPos);
                    const float Threshold = 50.f;
                    if (Dist2D > Threshold)
                    {
                        SwitchToRunState(ChunkContext, Entity, StateFrag);
                        continue;
                    }
                }
                else
                {
                }
            }
        }
    });
}

void UIdleStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const UWorld* World = EntityManager.GetWorld();
    if (!World || !SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto PathList = ChunkContext.GetFragmentView<FMassUnitPathFragment>();
        const bool bHasPathFrag = PathList.Num() > 0;

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassPatrolFragment& PatrolFrag = PatrolList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassUnitPathFragment* PathFrag = bHasPathFrag ? &PathList[i] : nullptr;

            if (StateFrag.SwitchingState) continue;

            const bool bPathActive = PathFrag && PathFrag->Waypoints.Num() > PathFrag->CurrentIndex;
            const bool bShouldIgnoreEnemies = bPathActive && !PathFrag->bAttackToggled;
            const bool bIsTargetActive = EntityManager.IsEntityActive(TargetFrag.TargetEntity);

            if (TargetFrag.bHasValidTarget && bIsTargetActive && !StateFrag.HoldPosition && !bShouldIgnoreEnemies)
            {
                SwitchToChaseState(ChunkContext, Entity, StateFrag);
                continue;
            }

            if (TargetFrag.bHasValidTarget && bIsTargetActive && StateFrag.HoldPosition)
            {
                const float EffectiveAttackRange = StatsFrag.AttackRange;
                const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    SwitchToPauseState(ChunkContext, Entity, StateFrag);
                    continue;
                }
            }

            if (this->bFollowTickThisFrame)
            {
                const bool bIsFriendlyActive = EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity);
                if (bIsFriendlyActive)
                {
                    FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                    if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                    {
                        FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                    }

                    const float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
                    FVector ToSelf2D = (Transform.GetLocation() - FriendlyLoc);
                    ToSelf2D.Z = 0.f;
                    const float Len2D = ToSelf2D.Size2D();
                    const FVector Dir2D = (Len2D > KINDA_SMALL_NUMBER) ? (ToSelf2D / Len2D) : FVector::XAxisVector;
                    FVector DesiredPos = FriendlyLoc + Dir2D * FollowRadius;

                    float OffsetMag = FMath::Clamp(TargetFrag.FollowOffset, 0.f, FollowRadius);
                    if (OffsetMag > 0.f)
                    {
                        uint64 Seed = (uint64)Entity.Index | ((uint64)Entity.SerialNumber << 32);
                        Seed += 0x9E3779B97F4A7C15ull;
                        Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
                        Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
                        Seed ^= (Seed >> 31);
                        const double Unit = (double)(Seed >> 11) * (1.0 / 9007199254740992.0);
                        const float Angle = (float)(Unit * 2.0 * PI);
                        const float CosA = FMath::Cos(Angle);
                        const float SinA = FMath::Sin(Angle);
                        DesiredPos.X += CosA * OffsetMag;
                        DesiredPos.Y += SinA * OffsetMag;
                    }

                    const float Dist2D = FVector::Dist2D(Transform.GetLocation(), DesiredPos);
                    const float Threshold = 50.f;
                    if (Dist2D > Threshold)
                    {
                        SwitchToRunState(ChunkContext, Entity, StateFrag);
                        continue;
                    }
                }
                else
                {
                }
            }

            StateFrag.StateTimer += ExecutionInterval;
            
            bool bHasPatrolRoute = PatrolFrag.CurrentWaypointIndex != INDEX_NONE;
            bool bIsOnPlattform = false;
            
            if (!bIsOnPlattform && PatrolFrag.bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= PatrolFrag.SetUnitsBackToPatrolTime)
            {
                SwitchToPatrolRandomState(ChunkContext, Entity, StateFrag);
                continue;
            }
        }
    });
}

void UIdleStateProcessor::SwitchToChaseState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
    auto& Defer = Context.Defer();

    if (StateFrag.CanAttack && StateFrag.IsInitialized)
    {
        Defer.AddTag<FMassStateDetectTag>(Entity);
    }
    
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
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

void UIdleStateProcessor::SwitchToPauseState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
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

void UIdleStateProcessor::SwitchToRunState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
{
    StateFrag.SwitchingState = true;
    if (Context.GetWorld() && Context.GetWorld()->IsNetMode(NM_Client))
    {
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

void UIdleStateProcessor::SwitchToPatrolRandomState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag)
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
