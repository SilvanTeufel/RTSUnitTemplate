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
    ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior; // Or a custom group
    ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks); // Example: Run after movement intent is set
    bRequiresGameThreadExecution = true;
}

void UGamePlayEffectProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    // Query 1: Find all entities that are casting an aura effect.
    CasterQuery.Initialize(EntityManager);
    CasterQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    CasterQuery.AddRequirement<FMassGameplayEffectFragment>(EMassFragmentAccess::ReadOnly);
    CasterQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    CasterQuery.AddTagRequirement<FMassStopGameplayEffectTag>(EMassFragmentPresence::None);
    CasterQuery.RegisterWithProcessor(*this);

    // Query 2: Find all entities that could potentially be a target for these effects.
    TargetQuery.Initialize(EntityManager);
    TargetQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    TargetQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite); // ReadOnly is fine, we operate on the Actor, not the fragment
    TargetQuery.AddRequirement<FMassGameplayEffectTargetFragment>(EMassFragmentAccess::ReadWrite);
    TargetQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);

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
    //UE_LOG(LogTemp, Error, TEXT("GamePlayEffectProcessor::Execute!!!!"));
    // --- STEP 1: Gather all active casters and their properties ---
    
    TArray<FCasterData> CasterDataList;
    CasterQuery.ForEachEntityChunk(Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassGameplayEffectFragment> EffectFragments = ChunkContext.GetFragmentView<FMassGameplayEffectFragment>();
        const TConstArrayView<FMassCombatStatsFragment> CombatStatsFragments = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        
       
        int NumCasters = ChunkContext.GetNumEntities();
            
        for (int32 i = 0; i < NumCasters; ++i)
        {
            FCasterData& Caster = CasterDataList.AddDefaulted_GetRef();
            Caster.Position = TransformFragments[i].GetTransform().GetLocation();
            Caster.RadiusSq = FMath::Square(EffectFragments[i].EffectRadius);
            Caster.TeamId = CombatStatsFragments[i].TeamId;
            
            if (EffectFragments[i].FriendlyEffect)
            {
                Caster.FriendlyEffect = EffectFragments[i].FriendlyEffect;
            }
            if (EffectFragments[i].EnemyEffect)
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
            TArrayView<FMassGameplayEffectTargetFragment> EffectTargetFragments = ChunkContext.GetMutableFragmentView<FMassGameplayEffectTargetFragment>();

        int NumTargets = ChunkContext.GetNumEntities();
            
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
                    
                    if (bIsFriendly && Caster.FriendlyEffect && !EffectTargetFragments[TargetIndex].FriendlyEffectApplied)
                    {
                        EffectTargetFragments[TargetIndex].FriendlyEffectApplied = true;
                        UnitBase->ApplyInvestmentEffect(Caster.FriendlyEffect);
                    }
                    else if (!bIsFriendly && Caster.EnemyEffect && !EffectTargetFragments[TargetIndex].EnemyEffectApplied)
                    {
                        EffectTargetFragments[TargetIndex].EnemyEffectApplied = true;
                        UnitBase->ApplyInvestmentEffect(Caster.EnemyEffect);
                    }
                }
            }


            if (EffectTargetFragments[TargetIndex].FriendlyEffectApplied)
            {
                EffectTargetFragments[TargetIndex].LastFriendlyEffectTime += ExecutionInterval;
                if (EffectTargetFragments[TargetIndex].LastFriendlyEffectTime >= EffectTargetFragments[TargetIndex].FriendlyEffectCoolDown)
                {
                    EffectTargetFragments[TargetIndex].LastFriendlyEffectTime = 0.0f;
                    EffectTargetFragments[TargetIndex].FriendlyEffectApplied = false;
                }
            }

            if (EffectTargetFragments[TargetIndex].EnemyEffectApplied)
            {
                EffectTargetFragments[TargetIndex].LastEnemyEffectTime += ExecutionInterval;
                if (EffectTargetFragments[TargetIndex].LastEnemyEffectTime >= EffectTargetFragments[TargetIndex].EnemyEffectCoolDown)
                {
                    EffectTargetFragments[TargetIndex].LastEnemyEffectTime = 0.0f;
                    EffectTargetFragments[TargetIndex].EnemyEffectApplied = false;
                }
            }
        }
    });
}