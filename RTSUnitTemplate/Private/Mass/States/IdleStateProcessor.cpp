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
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
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
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
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

    // Throttle follow assignment checks to once per second
    static float FollowAccum = 0.0f;
    FollowAccum += ExecutionInterval;
    const bool bFollowTickThisFrame = (FollowAccum >= 1.0f);
    if (bFollowTickThisFrame)
    {
        FollowAccum = 0.0f;
    }
    
    const UWorld* World = EntityManager.GetWorld(); // Use EntityManager consistently
    if (!World) return;

    if (!SignalSubsystem) return;
    
    EntityQuery.ForEachEntityChunk(Context,

        [this, World, &EntityManager, bFollowTickThisFrame](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            const FMassPatrolFragment& PatrolFrag = PatrolList[i];
            FMassVelocityFragment& VelocityFrag = VelocityList[i];
            
            VelocityFrag.Value = FVector::ZeroVector;
            
            

            if (TargetFrag.bHasValidTarget && !StateFrag.SwitchingState && !StateFrag.HoldPosition)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                }
                continue;
            }

            if (TargetFrag.bHasValidTarget && !StateFrag.SwitchingState && StateFrag.HoldPosition)
            {
                const float EffectiveAttackRange = StatsFrag.AttackRange;
    
              const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                        
              const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

              // --- In Attack Range ---
              if (DistSq <= AttackRangeSq && !StateFrag.SwitchingState)
              {
                  StateFrag.SwitchingState = true;
                  if (SignalSubsystem)
                  {
                      SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Pause, Entity);
                  }
                  continue;
              }
            }

                      // If following a friendly target, evaluate desired follow position (ring + optional offset)
            if (bFollowTickThisFrame && !StateFrag.SwitchingState && SignalSubsystem)
            {
                const bool bHasFriendly = EntityManager.IsEntityValid(TargetFrag.FriendlyTargetEntity);
                if (bHasFriendly)
                {
                    // Friendly location
                    FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
                    if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
                    {
                        FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
                    }

                    // Desired ring position at FollowRadius (XY only)
                    const float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
                    FVector ToSelf2D = (Transform.GetLocation() - FriendlyLoc);
                    ToSelf2D.Z = 0.f;
                    const float Len2D = ToSelf2D.Size2D();
                    const FVector Dir2D = (Len2D > KINDA_SMALL_NUMBER) ? (ToSelf2D / Len2D) : FVector::XAxisVector;
                    FVector DesiredPos = FriendlyLoc + Dir2D * FollowRadius;

                    // Apply optional random XY offset clamped to the radius
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

                    const float Dist2D = FVector::Dist2D(Transform.GetLocation(), DesiredPos);
                    const float Threshold = 20.f; // small hysteresis to avoid oscillation
                    if (Dist2D > Threshold)
                    {
                        StateFrag.SwitchingState = true;
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Run, Entity);
                        continue;
                    }
                }
            }

            StateFrag.StateTimer += ExecutionInterval;
            
            bool bHasPatrolRoute = PatrolFrag.CurrentWaypointIndex != INDEX_NONE;
            bool bIsOnPlattform = false;
            
            if (!bIsOnPlattform && !StateFrag.SwitchingState && PatrolFrag.bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= PatrolFrag.SetUnitsBackToPatrolTime)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::PatrolRandom, Entity);
                }
                continue;
            }

        } // End Entity Loop
    }); // End ForEachEntityChunk


}
