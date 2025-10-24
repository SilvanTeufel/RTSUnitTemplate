// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/ChaseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationTypes.h"

#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"

#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Controller\PlayerController\CustomControllerBase.h"


UChaseStateProcessor::UChaseStateProcessor(): EntityQuery()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UChaseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::All); // Nur Chase-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

    EntityQuery.RegisterWithProcessor(*this);
}

void UChaseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


FVector CalculateChaseOffset(const FMassEntityHandle& Entity, float MinRadius = 100.0f, float MaxRadius = 250.0f)
{
    if (!Entity.IsSet() || MinRadius >= MaxRadius || MinRadius < 0.f)
    {
        return FVector::ZeroVector;
    }

    // Use the entity index to seed our "randomness" deterministically.
    const uint32 Index = Entity.Index;

    // Use parts of the Golden Angle (137.5 degrees) for good angular distribution.
    const float AngleDeg = FMath::Fmod(static_cast<float>(Index) * 137.50776405f, 360.0f);

    // Vary the radius based on the index, ensuring it's within bounds.
    // We use a different multiplier to avoid radius aligning perfectly with angle for sequential indices.
    const float RadiusRange = MaxRadius - MinRadius;
    const float Radius = MinRadius + FMath::Fmod(static_cast<float>(Index) * 61.803398875f, RadiusRange);

    // Convert to radians for Cos/Sin
    const float AngleRad = FMath::DegreesToRadians(AngleDeg);

    // Calculate offset in X/Y plane
    return FVector(Radius * FMath::Cos(AngleRad),
                   Radius * FMath::Sin(AngleRad),
                   0.0f);
}

void UChaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Get World and Signal Subsystem once
    UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    if (!SignalSubsystem) return;

    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(Context,
        // Capture PendingSignals by reference. Capture World for helper functions.
        // Do NOT capture LocalSignalSubsystem directly here.
        [this, &PendingSignals, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop

            //UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& Stats = StatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for Update/Stop

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Target Lost ---
            StateFrag.StateTimer += ExecutionInterval;

            if (!Stats.bUseProjectile) // && TargetFrag.bHasValidTarget
                PendingSignals.Emplace(Entity, UnitSignals::UseRangedAbilitys);

            if (StateFrag.HoldPosition)
            {
                PendingSignals.Emplace(Entity, UnitSignals::Idle);
                continue;
            }
            
            if (!EntityManager.IsEntityValid(TargetFrag.TargetEntity) || (!TargetFrag.bHasValidTarget && !StateFrag.SwitchingState))
            {
                // Queue signal instead of sending directly
                UpdateMoveTarget(
                 MoveTarget,
                 StateFrag.StoredLocation,
                 Stats.RunSpeed,
                 World);
                SignalSubsystem->SignalEntity(UnitSignals::MirrorMoveTarget, Entity);
                
                StateFrag.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::SetUnitStatePlaceholder);
                continue;
            }

            // --- Distance Check ---
    
            const float DistSq = FVector::DistSquared2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

            FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity);
            const float EffectiveAttackRange = Stats.AttackRange+TargetCharFrag->CapsuleRadius/2.f;
            const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

            // --- In Attack Range ---
            if (DistSq <= AttackRangeSq && !StateFrag.SwitchingState)
            {
                // Queue signal instead of sending directly
                StopMovement(MoveTarget, World);
                SignalSubsystem->SignalEntity(UnitSignals::MirrorStopMovement, Entity);
                StateFrag.SwitchingState = true;
                PendingSignals.Emplace(Entity, UnitSignals::Pause);
                continue;
            }


           // You might want to adjust Min/Max Radius based on unit size or target.
           FVector ChaseOffset = CalculateChaseOffset(Entity, 0.0f, 50.0f);

           StateFrag.StoredLocation = TargetFrag.LastKnownLocation;
           UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, Stats.RunSpeed, World);
           SignalSubsystem->SignalEntity(UnitSignals::MirrorMoveTarget, Entity);

        }
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            // Capture the weak subsystem pointer and move the pending signals list
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                // Check if the subsystem is still valid on the Game Thread
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSignalPayload& Payload : SignalsToSend)
                    {
                        // Check if the FName is valid before sending
                        if (!Payload.SignalName.IsNone())
                        {
                           // Send signal safely from the Game Thread using FName
                           StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                        }
                    }
                }
            });
        }
    }
}
