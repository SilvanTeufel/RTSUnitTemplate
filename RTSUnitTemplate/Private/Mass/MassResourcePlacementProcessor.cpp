// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassResourcePlacementProcessor.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassCommonTypes.h"
#include "MassCommonFragments.h"

UMassResourcePlacementProcessor::UMassResourcePlacementProcessor() {
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
}

void UMassResourcePlacementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) {
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCarriedResourceFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this);
}

void UMassResourcePlacementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) {
    TSet<UInstancedStaticMeshComponent*> AffectedISMs;

    EntityQuery.ForEachEntityChunk(EntityManager, Context, ([&AffectedISMs](FMassExecutionContext& Context) {
        TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
        TArrayView<FMassCarriedResourceFragment> ResourceList = Context.GetMutableFragmentView<FMassCarriedResourceFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();

        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassActorFragment& ActorFrag = ActorList[i];
            FMassCarriedResourceFragment& ResourceFrag = ResourceList[i];
            const FMassVisibilityFragment& Vis = VisibilityList[i];

            bool bUnitVisible = Vis.bIsVisibleEnemy && Vis.bIsOnViewport;

            if (ResourceFrag.bIsCarrying && ResourceFrag.TargetISM.IsValid()) {
                UInstancedStaticMeshComponent* ISM = ResourceFrag.TargetISM.Get();
                AffectedISMs.Add(ISM);
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
                    
                    bool bTeleport = !ResourceFrag.bWasVisible;
                    ISM->UpdateInstanceTransform(ResourceFrag.InstanceIndex, SocketTransform, true, false, bTeleport);
                    ResourceFrag.bWasVisible = true;
                } else {
                    // Hide if worker is hidden, invalid or not visible
                    ISM->UpdateInstanceTransform(ResourceFrag.InstanceIndex, FTransform::Identity, true, false, true);
                    ResourceFrag.bWasVisible = false;
                }
            } else if (ResourceFrag.TargetISM.IsValid()) {
                // Ensure it's hidden if we stopped carrying
                AffectedISMs.Add(ResourceFrag.TargetISM.Get());
                ResourceFrag.TargetISM->UpdateInstanceTransform(ResourceFrag.InstanceIndex, FTransform::Identity, true, false, true);
                ResourceFrag.bWasVisible = false;
            }
        }
    }));

    for (UInstancedStaticMeshComponent* ISM : AffectedISMs) {
        if (ISM) {
            ISM->MarkRenderStateDirty();
        }
    }
}
