// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassUnitPlacementProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "MassRepresentationFragments.h"
#include "GameFramework/Actor.h"
#include "Characters/Unit/UnitBase.h"

UMassUnitPlacementProcessor::UMassUnitPlacementProcessor() {
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    ExecutionOrder.ExecuteAfter.Add(TEXT("ActorTransformSyncProcessor"));
}

void UMassUnitPlacementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) {
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassUnitVisualFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this);
}

// Collected ISM instance update for batched dispatch
struct FISMInstanceUpdate
{
    int32 InstanceIndex;
    FTransform NewTransform;
    bool bTeleport;
};

void UMassUnitPlacementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) {
    // Batch-Update: collect updates per ISM component instead of calling UpdateInstanceTransform individually
    TMap<UInstancedStaticMeshComponent*, TArray<FISMInstanceUpdate>> BatchedUpdates;

    EntityQuery.ForEachEntityChunk(Context, ([&EntityManager, &BatchedUpdates](FMassExecutionContext& Context) {
        TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        TArrayView<FMassUnitVisualFragment> VisualList = Context.GetMutableFragmentView<FMassUnitVisualFragment>();
        TConstArrayView<FMassVisualEffectFragment> EffectList = Context.GetFragmentView<FMassVisualEffectFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();
        TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        TConstArrayView<FMassRepresentationLODFragment> LODFragments = Context.GetFragmentView<FMassRepresentationLODFragment>();

        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassUnitVisualFragment& VisualFrag = VisualList[i];
            const FMassVisualEffectFragment& EffectFrag = EffectList[i];
            const FMassVisibilityFragment& Vis = VisibilityList[i];

            // LOD-Skip: Entities with LOD::Off get their ISMs hidden and are skipped
            if (LODFragments[i].LOD == EMassLOD::Off)
            {
                for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances)
                {
                    if (Instance.bWasVisible && Instance.TargetISM.IsValid())
                    {
                        FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
                        FISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(Instance.TargetISM.Get()).AddDefaulted_GetRef();
                        Update.InstanceIndex = Instance.InstanceIndex;
                        Update.NewTransform = HiddenTransform;
                        Update.bTeleport = true;
                        Instance.bWasVisible = false;
                    }
                }
                continue;
            }

            const AActor* Actor = ActorList[i].Get();
            const AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            FTransform BaseTransform;

            if (UnitBase && !UnitBase->bUseSkeletalMovement)
            {
                BaseTransform = CharList[i].PositionedTransform;
            }
            else if (Actor) {
                BaseTransform = Actor->GetActorTransform();
            } else {
                BaseTransform = TransformList[i].GetTransform();
            }

            bool bVisible = Vis.bIsVisibleEnemy && Vis.bIsOnViewport && !EffectFrag.bForceHidden;

            // Dirty-Flag check: skip entities whose transform hasn't changed and whose visibility is unchanged
            if (!CharList[i].bTransformDirty && !VisualFrag.bUseSkeletalMovement)
            {
                bool bVisibilityChanged = false;
                for (const FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances)
                {
                    if (Instance.bWasVisible != bVisible)
                    {
                        bVisibilityChanged = true;
                        break;
                    }
                }
                if (!bVisibilityChanged)
                {
                    continue;
                }
            }

            // Reset dirty flag after processing
            CharList[i].bTransformDirty = false;

            if (VisualFrag.bUseSkeletalMovement) {
                for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances) {
                    if (Instance.TargetISM.IsValid() && Instance.TemplateISM.IsValid()) {
                        FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
                        FISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(Instance.TargetISM.Get()).AddDefaulted_GetRef();
                        Update.InstanceIndex = Instance.InstanceIndex;
                        Update.NewTransform = HiddenTransform;
                        Update.bTeleport = true;
                        Instance.bWasVisible = false;
                    }
                }
                continue;
            }

            for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances) {
                if (Instance.TargetISM.IsValid() && Instance.TemplateISM.IsValid()) {
                    if (bVisible) {
                        FTransform FinalTransform = Instance.CurrentRelativeTransform * BaseTransform;
                        bool bTeleport = !Instance.bWasVisible;
                        FISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(Instance.TargetISM.Get()).AddDefaulted_GetRef();
                        Update.InstanceIndex = Instance.InstanceIndex;
                        Update.NewTransform = FinalTransform;
                        Update.bTeleport = bTeleport;
                        Instance.bWasVisible = true;
                    } else {
                        FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
                        FISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(Instance.TargetISM.Get()).AddDefaulted_GetRef();
                        Update.InstanceIndex = Instance.InstanceIndex;
                        Update.NewTransform = HiddenTransform;
                        Update.bTeleport = true;
                        Instance.bWasVisible = false;
                    }
                }
            }
        }
    }));

    // Batched dispatch: sort by index for cache-friendliness, use BatchUpdateInstancesTransforms when contiguous
    for (auto& [ISM, Updates] : BatchedUpdates)
    {
        if (!ISM) continue;

        // Sort by InstanceIndex for cache-friendly access
        Updates.Sort([](const FISMInstanceUpdate& A, const FISMInstanceUpdate& B)
        {
            return A.InstanceIndex < B.InstanceIndex;
        });

        // Check if indices form a contiguous range
        bool bContiguous = Updates.Num() > 1;
        for (int32 j = 1; j < Updates.Num() && bContiguous; ++j)
        {
            if (Updates[j].InstanceIndex != Updates[j-1].InstanceIndex + 1)
            {
                bContiguous = false;
            }
        }

        if (bContiguous)
        {
            // Contiguous range — use BatchUpdateInstancesTransforms
            TArray<FTransform> Transforms;
            Transforms.Reserve(Updates.Num());
            bool bAnyTeleport = false;
            for (const FISMInstanceUpdate& U : Updates)
            {
                Transforms.Add(U.NewTransform);
                bAnyTeleport |= U.bTeleport;
            }
            ISM->BatchUpdateInstancesTransforms(
                Updates[0].InstanceIndex,
                MakeArrayView(Transforms),
                /*bWorldSpace=*/ true,
                /*bMarkRenderStateDirty=*/ false,
                bAnyTeleport);
        }
        else
        {
            // Non-contiguous — individual updates (but sorted for better cache usage)
            for (const FISMInstanceUpdate& U : Updates)
            {
                ISM->UpdateInstanceTransform(U.InstanceIndex, U.NewTransform, true, false, U.bTeleport);
            }
        }

        // Update only dynamic data (transforms) on GPU to maintain motion vectors for TAA/Motion Blur.
        ISM->MarkRenderDynamicDataDirty();
    }
}
