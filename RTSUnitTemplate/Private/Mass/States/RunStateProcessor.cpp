// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/RunStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

URunStateProcessor::URunStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void URunStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All); // Nur Entities im Run-Zustand
    
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);       // Aktuelle Position lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel-Daten lesen, Stoppen erfordert Schreiben
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
 TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    // Throttle follow updates to max once per second (local static accumulator)
    static float FollowAccum = 0.0f;
    FollowAccum += ExecutionInterval;
    const bool bFollowTickThisFrameLocal = (FollowAccum >= 1.0f);
    if (bFollowTickThisFrameLocal)
    {
        FollowAccum = 0.0f;
    }
    // propagate to member so ExecuteClient/Server can read it
    this->bFollowTickThisFrame = bFollowTickThisFrameLocal;
    // Get World and Signal Subsystem once
    
    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        //ExecuteRepClient(EntityManager, Context);
        static int32 GActorSyncExecTickCounter = 0;
        if ((++GActorSyncExecTickCounter % 60) == 0)
        {
            if (bShowLogs)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Client][URunStateProcessor] Execute tick"));
            }
        }
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }

}

void URunStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    // On client, only check for arrival and switch to Idle locally by adjusting tags via deferred commands.
    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // If already dead, do not modify any tags on client
            if (DoesEntityHaveTag(EntityManager, Entity, FMassStateDeadTag::StaticStruct()))
            {
                continue;
            }
            // If health is zero or below, mark as dead and skip further processing
            if (Stats.Health <= 0.f)
            {
                auto& Defer = ChunkContext.Defer();
                Defer.AddTag<FMassStateDeadTag>(Entity);
                continue;
            }

            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float AcceptanceRadius = MoveTarget.SlackRadius;

            // No legacy follow signal; client-side prediction may be handled elsewhere

            // Only arrival check on client (skip if following a unit)
            const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
            if (!bHasFriendly && FVector::Dist2D(CurrentLocation, FinalDestination) <= AcceptanceRadius)
            {
                StateFrag.SwitchingState = true;
                VelocityList[i].Value = FVector::ZeroVector;

                // Emulate ClientIdle signal by removing all state tags and adding Idle locally via deferred command buffer tied to ChunkContext
                auto& Defer = ChunkContext.Defer();
                Defer.RemoveTag<FMassStateRunTag>(Entity);
                Defer.RemoveTag<FMassStateChaseTag>(Entity);
                Defer.RemoveTag<FMassStateAttackTag>(Entity);
                Defer.RemoveTag<FMassStatePauseTag>(Entity);
                //Defer.RemoveTag<FMassStateDeadTag>(Entity);
                Defer.RemoveTag<FMassStatePatrolRandomTag>(Entity);
                Defer.RemoveTag<FMassStatePatrolIdleTag>(Entity);
                Defer.RemoveTag<FMassStateCastingTag>(Entity);
                Defer.RemoveTag<FMassStateIsAttackedTag>(Entity);
                Defer.RemoveTag<FMassStateGoToBaseTag>(Entity);
                Defer.RemoveTag<FMassStateGoToBuildTag>(Entity);
                Defer.RemoveTag<FMassStateBuildTag>(Entity);
                Defer.RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
                Defer.RemoveTag<FMassStateResourceExtractionTag>(Entity);

            	Defer.AddTag<FMassStateIdleTag>(Entity);
                continue;
            }
        }
    });
}

void URunStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    if (!SignalSubsystem) return;
    
    EntityQuery.ForEachEntityChunk(Context,

        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for Update/Stop
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float AcceptanceRadius = MoveTarget.SlackRadius; //150.f;

            StateFrag.StateTimer += ExecutionInterval;

            
            // Follow friendly target directly if assigned
            const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
            
            
            if (DoesEntityHaveTag(EntityManager,Entity, FMassStateDetectTag::StaticStruct()) &&
                TargetFrag.bHasValidTarget && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                }
            }else if (bHasFriendly && !StateFrag.SwitchingState)
            {
                // Determine the friendly's current location
                FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                if (const FTransformFragment* FriendlyTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                {
                    FriendlyLoc = FriendlyTransform->GetTransform().GetLocation();
                }

                // Desired ring position at FollowRadius away from the friendly (XY only)
                const float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
                FVector ToSelf2D = (CurrentMassTransform.GetLocation() - FriendlyLoc);
                ToSelf2D.Z = 0.f;
                const float Len2D = ToSelf2D.Size2D();
                const FVector Dir2D = (Len2D > KINDA_SMALL_NUMBER) ? (ToSelf2D / Len2D) : FVector::XAxisVector;
                FVector DesiredPos = FriendlyLoc + Dir2D * FollowRadius;

                // Apply optional random XY offset, clamped to radius
                float OffsetMag = FMath::Clamp(TargetFrag.FollowOffset, 0.f, FollowRadius);
                if (OffsetMag > 0.f)
                {
                    // Unique, deterministic angular offset per entity to avoid identical positions
                    uint64 Seed = (uint64)Entity.Index | ((uint64)Entity.SerialNumber << 32);
                    // SplitMix64 scramble for good distribution
                    Seed += 0x9E3779B97F4A7C15ull;
                    Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
                    Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
                    Seed ^= (Seed >> 31);
                    // Map to [0,1) with 53-bit precision, then to [0, 2pi)
                    const double Unit = (double)(Seed >> 11) * (1.0 / 9007199254740992.0);
                    const float Angle = (float)(Unit * 2.0 * PI);
                    const float CosA = FMath::Cos(Angle);
                    const float SinA = FMath::Sin(Angle);
                    DesiredPos.X += CosA * OffsetMag;
                    DesiredPos.Y += SinA * OffsetMag;
                }
                DesiredPos.Z = FriendlyLoc.Z; // ignore Z while following

                // Update MoveTarget towards the adjusted desired position; skip Idle arrival while following
                UpdateMoveTarget(MoveTarget, DesiredPos, Stats.RunSpeed, World);
            }
            else if (!bHasFriendly && FVector::Dist2D(CurrentLocation, FinalDestination) <= AcceptanceRadius)
            {
                StateFrag.SwitchingState = true;
                VelocityList[i].Value = FVector::ZeroVector;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                }
                continue;
            }
            
        }
    }); // End ForEachEntityChunk

}
