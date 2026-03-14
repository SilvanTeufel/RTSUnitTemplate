// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassResourcePlacementProcessor.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassCommonTypes.h"
#include "MassCommonFragments.h"

// Collected ISM instance update for batched dispatch
struct FResourceISMInstanceUpdate
{
    int32 InstanceIndex;
    FTransform NewTransform;
    bool bTeleport;
};

UMassResourcePlacementProcessor::UMassResourcePlacementProcessor() {
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    ExecutionOrder.ExecuteAfter.Add(TEXT("ActorTransformSyncProcessor"));
}

void UMassResourcePlacementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) {
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCarriedResourceFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this);
}

void UMassResourcePlacementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) {
    // Batch-Update: collect updates per ISM component instead of calling UpdateInstanceTransform individually
    TMap<UInstancedStaticMeshComponent*, TArray<FResourceISMInstanceUpdate>> BatchedUpdates;

    EntityQuery.ForEachEntityChunk(EntityManager, Context, ([&BatchedUpdates](FMassExecutionContext& Context) {
        TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
        TArrayView<FMassCarriedResourceFragment> ResourceList = Context.GetMutableFragmentView<FMassCarriedResourceFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();

        const FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);

        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassActorFragment& ActorFrag = ActorList[i];
            FMassCarriedResourceFragment& ResourceFrag = ResourceList[i];
            const FMassVisibilityFragment& Vis = VisibilityList[i];

            bool bUnitVisible = Vis.bIsVisibleEnemy && Vis.bIsOnViewport;

            if (ResourceFrag.bIsCarrying && ResourceFrag.TargetISM.IsValid()) {
                UInstancedStaticMeshComponent* ISM = ResourceFrag.TargetISM.Get();
                AActor* Actor = ActorFrag.GetMutable();
                AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(Actor);
                if (Worker && Worker->GetMesh() && !Worker->IsHidden() && bUnitVisible) {
                    static const FName SocketName(TEXT("ResourceSocket"));
                    FTransform SocketTransform = Worker->GetMesh()->GetSocketTransform(SocketName);
                    
                    if (!ResourceFrag.SocketOffset.IsNearlyZero()) {
                        FVector OffsetWorld = SocketTransform.TransformVector(ResourceFrag.SocketOffset);
                        SocketTransform.AddToTranslation(OffsetWorld);
                    }

                    SocketTransform.SetScale3D(ResourceFrag.ResourceScale);

                    if (!ResourceFrag.ResourceRotation.IsNearlyZero()) {
                        FQuat AdditionalRotation = ResourceFrag.ResourceRotation.Quaternion();
                        SocketTransform.SetRotation(SocketTransform.GetRotation() * AdditionalRotation);
                    }
                    
                    bool bTeleport = !ResourceFrag.bWasVisible;
                    FResourceISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(ISM).AddDefaulted_GetRef();
                    Update.InstanceIndex = ResourceFrag.InstanceIndex;
                    Update.NewTransform = SocketTransform;
                    Update.bTeleport = bTeleport;
                    ResourceFrag.bWasVisible = true;
                } else {
                    // Hide if worker is hidden, invalid or not visible
                    FResourceISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(ISM).AddDefaulted_GetRef();
                    Update.InstanceIndex = ResourceFrag.InstanceIndex;
                    Update.NewTransform = HiddenTransform;
                    Update.bTeleport = true;
                    ResourceFrag.bWasVisible = false;
                }
            } else if (ResourceFrag.TargetISM.IsValid()) {
                // Ensure it's hidden if we stopped carrying
                UInstancedStaticMeshComponent* ISM = ResourceFrag.TargetISM.Get();
                FResourceISMInstanceUpdate& Update = BatchedUpdates.FindOrAdd(ISM).AddDefaulted_GetRef();
                Update.InstanceIndex = ResourceFrag.InstanceIndex;
                Update.NewTransform = HiddenTransform;
                Update.bTeleport = true;
                ResourceFrag.bWasVisible = false;
            }
        }
    }));

    // Batched dispatch: sort by index for cache-friendliness, use BatchUpdateInstancesTransforms when contiguous
    for (auto& [ISM, Updates] : BatchedUpdates)
    {
        if (!ISM) continue;

        // Sort by InstanceIndex for cache-friendly access
        Updates.Sort([](const FResourceISMInstanceUpdate& A, const FResourceISMInstanceUpdate& B)
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
            for (const FResourceISMInstanceUpdate& U : Updates)
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
            for (const FResourceISMInstanceUpdate& U : Updates)
            {
                ISM->UpdateInstanceTransform(U.InstanceIndex, U.NewTransform, true, false, U.bTeleport);
            }
        }

        // Update only dynamic data (transforms) on GPU
        ISM->MarkRenderDynamicDataDirty();
    }
}
