// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Abilitys/GamePlayEffectProcessor.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"     // For FMassMoveTargetFragment
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h" // For EMassMovementAction
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"

UGamePlayEffectProcessor::UGamePlayEffectProcessor()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server;// Or GameThread as needed
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior; // Or a custom group
    ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks); // Example: Run after movement intent is set
    bRequiresGameThreadExecution = true;
}

void UGamePlayEffectProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    /*
    EntityQuery.Initialize(EntityManager);
 
    //RegisterQuery(EntityQuery);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassGameplayEffectFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassAddsFriendlyEffectTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassAddsEnemyEffectTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassGetEffectTag>(EMassFragmentPresence::Any);

    EntityQuery.RegisterWithProcessor(*this);
        */

    // Query 1: Find all entities that are casting an aura effect.
    CasterQuery.Initialize(EntityManager);
    CasterQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    CasterQuery.AddRequirement<FMassGameplayEffectFragment>(EMassFragmentAccess::ReadOnly);
    CasterQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    CasterQuery.AddTagRequirement<FMassAddsFriendlyEffectTag>(EMassFragmentPresence::Any);
    CasterQuery.AddTagRequirement<FMassAddsEnemyEffectTag>(EMassFragmentPresence::Any);
    CasterQuery.RegisterWithProcessor(*this);

    // Query 2: Find all entities that could potentially be a target for these effects.
    TargetQuery.Initialize(EntityManager);
    TargetQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    TargetQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite); // ReadOnly is fine, we operate on the Actor, not the fragment
    TargetQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    TargetQuery.AddTagRequirement<FMassGameplayEffectTargetTag>(EMassFragmentPresence::Any); // Ensure we only target valid entities
    TargetQuery.RegisterWithProcessor(*this);
}

void UGamePlayEffectProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Optional: Throttle the processor if it doesn't need to run every single frame.
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun = 0.0f; // Reset timer
    UE_LOG(LogTemp, Error, TEXT("GamePlayEffectProcessor::Execute!!!!"));
    // --- STEP 1: Gather all active casters and their properties ---
    
    TArray<FCasterData> CasterDataList;
    CasterQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassGameplayEffectFragment> EffectFragments = ChunkContext.GetFragmentView<FMassGameplayEffectFragment>();
        const TConstArrayView<FMassCombatStatsFragment> CombatStatsFragments = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        
        const bool bHasFriendlyTag = ChunkContext.DoesArchetypeHaveTag<FMassAddsFriendlyEffectTag>();
        const bool bHasEnemyTag = ChunkContext.DoesArchetypeHaveTag<FMassAddsEnemyEffectTag>();
            int NumCasters = ChunkContext.GetNumEntities();
            UE_LOG(LogTemp, Error, TEXT("NumCasters: %d"), NumCasters);
        for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
        {
            FCasterData& Caster = CasterDataList.AddDefaulted_GetRef();
            Caster.Position = TransformFragments[i].GetTransform().GetLocation();
            Caster.RadiusSq = FMath::Square(EffectFragments[i].EffectRadius);
            Caster.TeamId = CombatStatsFragments[i].TeamId;
            
            if (bHasFriendlyTag)
            {
                Caster.FriendlyEffect = EffectFragments[i].FriendlyEffect;
            }
            if (bHasEnemyTag)
            {
                Caster.EnemyEffect = EffectFragments[i].EnemyEffect;
            }
        }
    });

    // If there are no casters, there's nothing to do.
    if (CasterDataList.IsEmpty())
    {
        return;
    }

    // --- STEP 2: Iterate through all potential targets and apply effects from casters in range ---
    TargetQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const TConstArrayView<FTransformFragment> TargetTransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
            TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> CombatStatsFragments = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

            int NumTargets = ChunkContext.GetNumEntities();
           UE_LOG(LogTemp, Error, TEXT("NumCasters: %d"), NumTargets);
        for (int32 TargetIndex = 0; TargetIndex < NumTargets; ++TargetIndex)
        {
            
            const FVector TargetPosition = TargetTransformFragments[TargetIndex].GetTransform().GetLocation();
            const int32 TargetTeamId = CombatStatsFragments[TargetIndex].TeamId;
            AActor* Actor = ActorFragments[TargetIndex].GetMutable();
                AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!UnitBase)
            {
                continue;
            }

            // For each target, check against every caster
            for (const FCasterData& Caster : CasterDataList)
            {
                // A. Check Distance
                if (FVector::DistSquared(TargetPosition, Caster.Position) <= Caster.RadiusSq)
                {
                    // B. Check Team Alignment & Apply Effect
                    const bool bIsFriendly = (Caster.TeamId == TargetTeamId);
                    
                    if (bIsFriendly && Caster.FriendlyEffect)
                    {
      
                        UnitBase->ApplyInvestmentEffect(Caster.FriendlyEffect);
                        UE_LOG(LogTemp, Verbose, TEXT("Applied friendly effect from team %d to team %d."), Caster.TeamId, TargetTeamId);
                    }
                    else if (!bIsFriendly && Caster.EnemyEffect)
                    {
                        UnitBase->ApplyInvestmentEffect(Caster.EnemyEffect);
                        UE_LOG(LogTemp, Verbose, TEXT("Applied enemy effect from team %d to team %d."), Caster.TeamId, TargetTeamId);
                    }
                }
            }
        }
    });
}