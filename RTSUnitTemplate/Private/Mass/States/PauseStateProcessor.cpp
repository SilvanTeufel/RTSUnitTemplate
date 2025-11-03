// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/PauseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Controller\PlayerController\CustomControllerBase.h"

UPauseStateProcessor::UPauseStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UPauseStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All); // Nur Pause-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben, Zustand ändern
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Stats lesen (AttackPauseDuration)
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position für Distanzcheck
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UPauseStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UPauseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Get World and Signal Subsystem once
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager to get World
    if (!World) return;

    if (!SignalSubsystem) return;


    EntityQuery.ForEachEntityChunk(Context,
        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        auto SightList = ChunkContext.GetMutableFragmentView<FMassSightFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            FMassSightFragment& SightFrag = SightList[i];

            if (!StateFrag.CanAttack)
            {
               TargetFrag.TargetEntity.Reset();
               TargetFrag.bHasValidTarget = false;
            }
            
            if (!EntityManager.IsEntityValid(TargetFrag.TargetEntity) || !TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet() && !StateFrag.SwitchingState)
            {
                SightFrag.AttackerTeamOverlapsPerTeam.Empty();
                UpdateMoveTarget(
                 MoveTarget,
                 StateFrag.StoredLocation,
                 Stats.RunSpeed,
                 World);


                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SetUnitStatePlaceholder, Entity);
                }
                continue;
            }
            
            StateFrag.StateTimer += ExecutionInterval;
            
            FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.TargetEntity);
            FMassCombatStatsFragment* TargetStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetFrag.TargetEntity);
            
            const float Dist = FVector::Dist2D(Transform.GetLocation(), TargetFrag.LastKnownLocation);

            float AttackRange = Stats.AttackRange+TargetCharFrag->CapsuleRadius/2.f;
            
                if (Dist <= AttackRange) // --- In Range ---
                {
                    if (StateFrag.StateTimer >= Stats.PauseDuration  && !StateFrag.SwitchingState)
                    {
                        StateFrag.SwitchingState = true;
                        if (Stats.bUseProjectile)
                        {

                            if (SightFrag.AttackerTeamOverlapsPerTeam.FindOrAdd(TargetStats->TeamId) <= 0)
                                SightFrag.AttackerTeamOverlapsPerTeam.FindOrAdd(TargetStats->TeamId)++;
                            
                            if (SignalSubsystem)
                            {
                                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::RangedAttack, Entity);
                            }
                        }else
                        {
                            if (SignalSubsystem)
                            {
                                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Attack, Entity);
                            }
                        }
                    }
                }
                else if (!StateFrag.SwitchingState)
                {
                    SightFrag.AttackerTeamOverlapsPerTeam.Empty();
                    StateFrag.SwitchingState = true;
                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                    }
                }
        }
    }); // End ForEachEntityChunk
  

    
}
