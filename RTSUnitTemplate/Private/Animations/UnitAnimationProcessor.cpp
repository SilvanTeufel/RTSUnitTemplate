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
#include "Components/InstancedStaticMeshComponent.h"
#include "Characters/Unit/MassUnitBase.h"

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
    const float CurrentWorldTime = Context.GetWorld()->GetTimeSeconds();

    EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager, &Context, CurrentWorldTime](FMassExecutionContext& ChunkContext)
    {
        const TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassAIStateFragment> StateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassUnitVisualFragment> VisualList = ChunkContext.GetFragmentView<FMassUnitVisualFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TArrayView<FUnitAnimationFragment> AnimList = ChunkContext.GetMutableFragmentView<FUnitAnimationFragment>();

        for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
        {
            const FMassUnitVisualFragment& VisualFrag = VisualList[i];
            FUnitAnimationFragment& AnimFrag = AnimList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            
            if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorList[i].GetMutable()))
            {
                const TEnumAsByte<UnitData::EState> CurrentState = UnitBase->GetUnitState();

                // 1. Check for State Change and Update Targets
                if (AnimFrag.LastProcessedState != CurrentState)
                {
                    if (VisualFrag.bUseSkeletalMovement)
                    {
                        if (UUnitBaseAnimInstance* AnimInst = Cast<UUnitBaseAnimInstance>(UnitBase->GetMesh()->GetAnimInstance()))
                        {
                            if (AnimInst->AnimDataTable)
                            {
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
                                             break;
                                         }
                                     }
                                 }
                            }
                        }
                    }
                    else if (AnimFrag.ISMAnimationDataTable)
                    {
                        for (auto It : AnimFrag.ISMAnimationDataTable->GetRowMap())
                        {
                            if (FISMAnimationData* RowData = reinterpret_cast<FISMAnimationData*>(It.Value))
                            {
                                if (RowData->AnimState == CurrentState)
                                {
                                    AnimFrag.PrevTargetStateCustomDataValue = AnimFrag.TargetStateCustomDataValue;
                                    AnimFrag.PrevStartTime = AnimFrag.CurrentStartTime;
                                    AnimFrag.PrevStartFrame = AnimFrag.CurrentStartFrame;
                                    AnimFrag.PrevEndFrame = AnimFrag.CurrentEndFrame;
                                    AnimFrag.PrevPlayRate = AnimFrag.CurrentPlayRate;

                                    AnimFrag.TargetStateCustomDataValue = RowData->StateCustomDataValue;
                                    AnimFrag.TransitionRate_1 = RowData->TransitionRate;
                                    AnimFrag.CurrentStartTime = CurrentWorldTime;
                                    AnimFrag.CurrentStartFrame = RowData->StartFrame;
                                    AnimFrag.CurrentEndFrame = RowData->EndFrame;
                                    AnimFrag.CurrentPlayRate = RowData->PlayRate;

                                    AnimFrag.BlendAlpha = 0.0f;

                                    int32 InstanceIndex = INDEX_NONE;
                                    UInstancedStaticMeshComponent* TargetISM = nullptr;

                                    // 1. Attempt: Via VisualFragment
                                    if (VisualFrag.VisualInstances.Num() > 0)
                                    {
                                        InstanceIndex = VisualFrag.VisualInstances[0].InstanceIndex;
                                        TargetISM = VisualFrag.VisualInstances[0].TargetISM.Get();
                                    }

                                    // 2. Attempt: Fallback to UnitBase
                                    if (InstanceIndex == INDEX_NONE)
                                    {
                                        if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(UnitBase))
                                        {
                                            InstanceIndex = MassUnit->InstanceIndex;
                                            TargetISM = MassUnit->ISMComponent;
                                        }
                                    }

                                    if (TargetISM && InstanceIndex != INDEX_NONE)
                                    {
                                        // Automatische Korrektur der CustomData-Größe, falls zu klein
                                        int32 RequiredFloats = FMath::Max(StateCustomDataIndex, FMath::Max(TransitionRateCustomDataIndex, FMath::Max(StartTimeCustomDataIndex, FMath::Max(StartFrameCustomDataIndex, FMath::Max(EndFrameCustomDataIndex, FMath::Max(PrevStateCustomDataIndex, FMath::Max(PrevStartTimeCustomDataIndex, FMath::Max(PrevStartFrameCustomDataIndex, FMath::Max(PrevEndFrameCustomDataIndex, FMath::Max(BlendAlphaCustomDataIndex, FMath::Max(PlayRateCustomDataIndex, PrevPlayRateCustomDataIndex))))))))))) + 1;
                                        if (TargetISM->NumCustomDataFloats < RequiredFloats)
                                        {
                                            TargetISM->SetNumCustomDataFloats(RequiredFloats);
                                        }
                                        
                                        TargetISM->SetCustomDataValue(InstanceIndex, StateCustomDataIndex, AnimFrag.TargetStateCustomDataValue, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, TransitionRateCustomDataIndex, AnimFrag.TransitionRate_1, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, StartTimeCustomDataIndex, AnimFrag.CurrentStartTime, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, StartFrameCustomDataIndex, AnimFrag.CurrentStartFrame, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, EndFrameCustomDataIndex, AnimFrag.CurrentEndFrame, true);
                                        
                                        TargetISM->SetCustomDataValue(InstanceIndex, PrevStateCustomDataIndex, AnimFrag.PrevTargetStateCustomDataValue, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, PrevStartTimeCustomDataIndex, AnimFrag.PrevStartTime, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, PrevStartFrameCustomDataIndex, AnimFrag.PrevStartFrame, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, PrevEndFrameCustomDataIndex, AnimFrag.PrevEndFrame, true);
                                        
                                        TargetISM->SetCustomDataValue(InstanceIndex, BlendAlphaCustomDataIndex, AnimFrag.BlendAlpha, true);

                                        TargetISM->SetCustomDataValue(InstanceIndex, PlayRateCustomDataIndex, AnimFrag.CurrentPlayRate, true);
                                        TargetISM->SetCustomDataValue(InstanceIndex, PrevPlayRateCustomDataIndex, AnimFrag.PrevPlayRate, true);
                                    }
                                    break;
                                }
                            }
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

            if (AnimFrag.BlendAlpha < 1.0f)
            {
                AnimFrag.BlendAlpha = FMath::Clamp(AnimFrag.BlendAlpha + (DeltaTime * AnimFrag.TransitionRate_1), 0.0f, 1.0f);

                int32 InstanceIndex = INDEX_NONE;
                UInstancedStaticMeshComponent* TargetISM = nullptr;

                if (VisualFrag.VisualInstances.Num() > 0)
                {
                    InstanceIndex = VisualFrag.VisualInstances[0].InstanceIndex;
                    TargetISM = VisualFrag.VisualInstances[0].TargetISM.Get();
                }

                if (InstanceIndex == INDEX_NONE)
                {
                    if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(ActorList[i].GetMutable()))
                    {
                        InstanceIndex = MassUnit->InstanceIndex;
                        TargetISM = MassUnit->ISMComponent;
                    }
                }

                if (TargetISM && InstanceIndex != INDEX_NONE)
                {
                    TargetISM->SetCustomDataValue(InstanceIndex, BlendAlphaCustomDataIndex, AnimFrag.BlendAlpha, true);
                }
            }
        }
    });
}
