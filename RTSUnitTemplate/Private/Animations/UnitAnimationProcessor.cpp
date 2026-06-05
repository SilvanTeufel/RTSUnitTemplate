// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Animations/UnitAnimationProcessor.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "Animations/UnitBaseAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UUnitAnimationProcessor::UUnitAnimationProcessor()
{
    bRequiresGameThreadExecution = true;
    ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UUnitAnimationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassUnitVisualFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FUnitAnimationFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitAnimationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld();
    const FString NetModeStr = (World && World->GetNetMode() == NM_Client) ? TEXT("[Client]") : TEXT("[Server]");

    EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager, &Context, &NetModeStr](FMassExecutionContext& ChunkContext)
    {
        const TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassUnitVisualFragment> VisualList = ChunkContext.GetFragmentView<FMassUnitVisualFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TArrayView<FUnitAnimationFragment> AnimList = ChunkContext.GetMutableFragmentView<FUnitAnimationFragment>();

        for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
        {
            const FMassUnitVisualFragment& VisualFrag = VisualList[i];
            if (!VisualFrag.bUseSkeletalMovement)
            {
                continue;
            }

            FUnitAnimationFragment& AnimFrag = AnimList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            
            if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorList[i].GetMutable()))
            {
                const TEnumAsByte<UnitData::EState> CurrentState = UnitBase->GetUnitState();

                // 1. Check for State Change and Update Targets
                if (AnimFrag.LastProcessedState != CurrentState)
                {
                    /*
                    UE_LOG(LogTemp, Log, TEXT("%s [AnimationProcessor] State Change for %s: %s -> %s"), 
                        *NetModeStr,
                        *UnitBase->GetName(), 
                        *UEnum::GetValueAsString(AnimFrag.LastProcessedState.GetValue()), 
                        *UEnum::GetValueAsString(CurrentState.GetValue()));
                    */
                    if (UUnitBaseAnimInstance* AnimInst = Cast<UUnitBaseAnimInstance>(UnitBase->GetMesh()->GetAnimInstance()))
                    {
                        if (AnimInst->AnimDataTable)
                        {
                             bool bFound = false;
                             for (auto It : AnimInst->AnimDataTable->GetRowMap())
                             {
                                 if (FUnitAnimData* RowData = reinterpret_cast<FUnitAnimData*>(It.Value))
                                 {
                                     if (RowData->AnimState == CurrentState)
                                     {
                                         AnimFrag.TargetBlendPoint_1 = RowData->BlendPoint_1;
                                         AnimFrag.TargetBlendPoint_2 = RowData->BlendPoint_2;
                                         AnimFrag.TransitionRate_1 = RowData->TransitionRate_1;
                                         AnimFrag.TransitionRate_2 = RowData->TransitionRate_2;
                                         AnimFrag.Resolution_1 = RowData->Resolution_1;
                                         AnimFrag.Resolution_2 = RowData->Resolution_2;
                                         AnimFrag.Sound = RowData->Sound;
                                         bFound = true;

                                         //UE_LOG(LogTemp, Log, TEXT("%s [AnimationProcessor] Loaded Row for %s: BP1=%.2f, BP2=%.2f"), 
                                            // *NetModeStr, *UnitBase->GetName(), AnimFrag.TargetBlendPoint_1, AnimFrag.TargetBlendPoint_2);
                                         break;
                                     }
                                 }
                             }
                             if (!bFound)
                             {
                                 //UE_LOG(LogTemp, Warning, TEXT("%s [AnimationProcessor] No Row found in DataTable for State %s (Unit: %s)"), 
                                    // *NetModeStr, *UEnum::GetValueAsString(CurrentState.GetValue()), *UnitBase->GetName());
                             }
                        }
                        else
                        {
                           // UE_LOG(LogTemp, Warning, TEXT("%s [AnimationProcessor] AnimDataTable is NULL for %s"), *NetModeStr, *UnitBase->GetName());
                        }
                    }
                    AnimFrag.LastProcessedState = CurrentState;
                }
            }

            // 2. Interpolate Current Values
            const float DeltaTime = Context.GetDeltaTimeSeconds();

            if (DoesEntityHaveTag(EntityManager, ChunkContext.GetEntity(i), FMassStateContinuousAttackTag::StaticStruct()))
            {
                const FMassAIStateFragment& StateFrag = StateList[i];
                float Duration = Stats.ContinuousAttackDuration;
                if (Duration > 0.f)
                {
                    float SpeedMultiplier = 1.0f;
                    float StartDelayMultiplier = 1.0f;
                    float CycleRatio = 0.8f;

                    if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorList[i].GetMutable()))
                    {
                        if (UUnitBaseAnimInstance* AnimInst = Cast<UUnitBaseAnimInstance>(UnitBase->GetMesh()->GetAnimInstance()))
                        {
                            SpeedMultiplier = AnimInst->ContinuousAttackSpeedMultiplier;
                            StartDelayMultiplier = AnimInst->ContinuousAttackStartDelayMultiplier;
                            CycleRatio = AnimInst->ContinuousAttackCycleRatio;
                        }
                    }

                    AnimFrag.PlayRate = (1.0f / (Duration * CycleRatio)) * SpeedMultiplier;
                    
                    float StartDelay = Duration * StartDelayMultiplier;

                    if (StateFrag.StateTimerClient < StartDelay)
                    {
                        AnimFrag.AnimationPosition = 0.f;
                    }
                    else
                    {
                        float CycleTime = FMath::Fmod(StateFrag.StateTimerClient - StartDelay, Duration);
                        if (CycleTime < Duration * CycleRatio)
                        {
                            AnimFrag.AnimationPosition = FMath::Clamp(CycleTime / (Duration * CycleRatio), 0.f, 1.f);
                        }
                        else
                        {
                            AnimFrag.AnimationPosition = 1.f;
                        }
                    }
                }
            }
            else
            {
                AnimFrag.PlayRate = 1.0f;
                AnimFrag.AnimationPosition = 0.f;
            }

            AnimFrag.CurrentBlendPoint_1 = FMath::FInterpTo(AnimFrag.CurrentBlendPoint_1, AnimFrag.TargetBlendPoint_1, DeltaTime, AnimFrag.TransitionRate_1);
            AnimFrag.CurrentBlendPoint_2 = FMath::FInterpTo(AnimFrag.CurrentBlendPoint_2, AnimFrag.TargetBlendPoint_2, DeltaTime, AnimFrag.TransitionRate_2);

            if (FMath::Abs(AnimFrag.CurrentBlendPoint_1 - AnimFrag.TargetBlendPoint_1) <= AnimFrag.Resolution_1)
            {
                AnimFrag.CurrentBlendPoint_1 = AnimFrag.TargetBlendPoint_1;
            }
            if (FMath::Abs(AnimFrag.CurrentBlendPoint_2 - AnimFrag.TargetBlendPoint_2) <= AnimFrag.Resolution_2)
            {
                AnimFrag.CurrentBlendPoint_2 = AnimFrag.TargetBlendPoint_2;
            }

            if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorList[i].GetMutable()))
            {
                if (UWorld* World = UnitBase->GetWorld())
                {
                    if (World->GetNetMode() == NM_Client)
                    {
                        // Log roughly every second if CBP changes (just for debugging)
                        static float LogTimer = 0.f;
                        LogTimer += Context.GetDeltaTimeSeconds();
                        if (LogTimer > 1.0f)
                        {
                             // UE_LOG(LogTemp, Log, TEXT("[AnimationProcessor-Client] %s: CBP1=%.2f, Target1=%.2f"), *UnitBase->GetName(), AnimFrag.CurrentBlendPoint_1, AnimFrag.TargetBlendPoint_1);
                             // LogTimer = 0.f;
                        }
                    }
                }
            }
        }
    });
}
