// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassUnitPlacementProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
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
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this);
}

void UMassUnitPlacementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) {
    TSet<UInstancedStaticMeshComponent*> AffectedISMs;

    static double LastLogTime = 0.0;
    static int32 LoggedThisFrame = 0;
    double CurrentTime = FPlatformTime::Seconds();
    bool bShouldLog = (CurrentTime - LastLogTime) > 0.5;
    if (bShouldLog) {
        LastLogTime = CurrentTime;
        LoggedThisFrame = 0;
    }

    EntityQuery.ForEachEntityChunk(Context, ([&EntityManager, &AffectedISMs, bShouldLog](FMassExecutionContext& Context) {
        TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        TArrayView<FMassUnitVisualFragment> VisualList = Context.GetMutableFragmentView<FMassUnitVisualFragment>();
        TConstArrayView<FMassVisualEffectFragment> EffectList = Context.GetFragmentView<FMassVisualEffectFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();
        TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
        TConstArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassUnitVisualFragment& VisualFrag = VisualList[i];
            const FMassVisualEffectFragment& EffectFrag = EffectList[i];
            const FMassVisibilityFragment& Vis = VisibilityList[i];

            const AActor* Actor = ActorList[i].Get();
            const AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            FTransform BaseTransform;
            FString BaseSource = TEXT("Actor");

            if (UnitBase && !UnitBase->bUseSkeletalMovement)
            {
                BaseTransform = CharList[i].PositionedTransform;
                BaseSource = TEXT("CharFragment");
            }
            else if (Actor) {
                BaseTransform = Actor->GetActorTransform();
            } else {
                BaseTransform = TransformList[i].GetTransform();
                BaseSource = TEXT("Fragment");
            }

            bool bVisible = Vis.bIsVisibleEnemy && Vis.bIsOnViewport && !EffectFrag.bForceHidden;

            if (VisualFrag.bUseSkeletalMovement) {
                for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances) {
                    if (Instance.TargetISM.IsValid() && Instance.TemplateISM.IsValid()) {
                        AffectedISMs.Add(Instance.TargetISM.Get());
                        // Hide skeletal movement units by setting scale to 0
                        FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
                        Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, HiddenTransform, true, false, true);
                        Instance.bWasVisible = false;
                    }
                }
                continue;
            }

            for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances) {
                if (Instance.TargetISM.IsValid() && Instance.TemplateISM.IsValid()) {
                    AffectedISMs.Add(Instance.TargetISM.Get());

                    if (bVisible) {
                        FTransform FinalTransform = Instance.CurrentRelativeTransform * BaseTransform;
                        bool bTeleport = !Instance.bWasVisible;
                        // Use bMarkRenderStateDirty = false to preserve Temporal History (TAA/TSR) and avoid ghosting.
                        Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, FinalTransform, true, false, bTeleport);
                        Instance.bWasVisible = true;

                        if (bShouldLog && LoggedThisFrame < 10) {
                            LoggedThisFrame++;
                            /*UE_LOG(LogTemp, Log, TEXT("Placement: Entity %s, ISM: %s, RelLoc: %s, RelRot: %s, BaseLoc: %s (%s), FinalLoc: %s"), 
                                *Context.GetEntity(i).DebugGetDescription(), *Instance.TargetISM->GetName(), 
                                *Instance.CurrentRelativeTransform.GetLocation().ToString(), 
                                *Instance.CurrentRelativeTransform.GetRotation().Rotator().ToString(),
                                *BaseTransform.GetLocation().ToString(),
                                *BaseSource,
                                *FinalTransform.GetLocation().ToString());*/
                        }
                    } else {
                        // Bei Unsichtbarkeit (z.B. außerhalb des Viewports) wird das ISM an den Nullpunkt verschoben und auf Scale 0 gesetzt.
                        FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
                        Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, HiddenTransform, true, false, true);
                        Instance.bWasVisible = false;
                    }
                }
            }
        }
    }));

    for (UInstancedStaticMeshComponent* ISM : AffectedISMs) {
        if (ISM) {
            // Update only dynamic data (transforms) on GPU to maintain motion vectors for TAA/Motion Blur.
            ISM->MarkRenderDynamicDataDirty();
        }
    }
}
