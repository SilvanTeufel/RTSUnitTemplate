// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/AttackStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h" // Für Schadens-Signal

// Fragmente und Tags
#include "MassActorSubsystem.h"   
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassActorSubsystem.h"  
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Controller\PlayerController\CustomControllerBase.h"


UAttackStateProcessor::UAttackStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UAttackStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UAttackStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None); // Exclude Chase too
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None); // Already excluded by other logic, but explicit

    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Ensure the member SignalSubsystem is valid (initialized in Initialize)
    if (!SignalSubsystem) return;

    UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;
    EntityQuery.ForEachEntityChunk(Context,
        // Use deferred signaling via ChunkContext; do NOT dispatch AsyncTask here
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // State modification stays here
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for UpdateMoveTarget

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Target Lost ---
            if (!EntityManager.IsEntityValid(TargetFrag.TargetEntity) || !TargetFrag.bHasValidTarget || (!TargetFrag.TargetEntity.IsSet() && !StateFrag.SwitchingState))
            {
                  // Queue signal instead of sending directly
                  UpdateMoveTarget(
                   MoveTarget,
                   StateFrag.StoredLocation,
                   Stats.RunSpeed,
                   World);
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::MirrorMoveTarget, Entity);
                }
                
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SetUnitStatePlaceholder, Entity);
                }
                continue;
            }
            
            // --- Attack Cycle Timer ---
            StateFrag.StateTimer += ExecutionInterval; // State modification stays here
            
                // --- Range Check ---
                //const float Dist = FVector::Dist(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

                FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity);
                const float AttackRange = Stats.AttackRange+TargetCharFrag->CapsuleRadius/2.f;
            
                if (Dist <= AttackRange)
                {
                    // --- Melee Impact Check ---
                    if (StateFrag.StateTimer <= Stats.AttackDuration)
                    {
                        if (!Stats.bUseProjectile && !StateFrag.HasAttacked)
                        {
                            if (SignalSubsystem)
                            {
                                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::MeleeAttack, Entity);
                            }
                            StateFrag.HasAttacked = true; // State modification stays here
                        }
                    }else if (!StateFrag.SwitchingState) // --- Attack Duration Over ---
                    {
                        StateFrag.SwitchingState = true;
                        if (SignalSubsystem)
                        {
                            SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Pause, Entity);
                        }
                        StateFrag.HasAttacked = false; // State modification stays here
                        continue;
                    }
                }
                else if (!StateFrag.SwitchingState)
                {
                    StateFrag.SwitchingState = true;
                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                    }
                    continue;
                }
      
        }
    }); // End ForEachEntityChunk


}