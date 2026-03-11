// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassUnitPlacementProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "GameFramework/Actor.h"

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
    EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this);
}

void UMassUnitPlacementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) {
    EntityQuery.ForEachEntityChunk(Context, ([&EntityManager](FMassExecutionContext& Context) {
        TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        TArrayView<FMassUnitVisualFragment> VisualList = Context.GetMutableFragmentView<FMassUnitVisualFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();
        TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
        TConstArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassUnitVisualFragment& VisualFrag = VisualList[i];
            const FMassVisibilityFragment& Vis = VisibilityList[i];

            const AActor* Actor = ActorList[i].Get();
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            FTransform BaseTransform;
            if (Actor) {
                BaseTransform = Actor->GetActorTransform();
                
                // Der Actor-Transform wird bevorzugt, da dieser im ActorTransformSyncProcessor bereits
                // korrekt am Boden ausgerichtet, geneigt und sanft interpoliert wurde.
            } else {
                BaseTransform = TransformList[i].GetTransform();
            }

            bool bVisible = Vis.bIsVisibleEnemy && Vis.bIsOnViewport;

            if (VisualFrag.bUseSkeletalMovement) {
                for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances) {
                    if (Instance.TargetISM.IsValid()) {
                        // Auch bei SkeletalMovement sicherstellen, dass ISMs nicht stören, falls sie aktiv wären.
                        if (Instance.TargetISM->GetCollisionObjectType() == ECC_WorldStatic) {
                            Instance.TargetISM->SetCollisionObjectType(ECC_WorldDynamic);
                        }
                        Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true);
                    }
                }
                continue;
            }

            for (FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances) {
                if (Instance.TargetISM.IsValid()) {
                    // Die gepoolten ISMs dürfen nicht WorldStatic sein, da sie sonst
                    // die Bodenprüfung (Line-Trace) ihrer eigenen Einheiten stören.
                    if (Instance.TargetISM->GetCollisionObjectType() == ECC_WorldStatic) {
                        Instance.TargetISM->SetCollisionObjectType(ECC_WorldDynamic);
                    }

                    if (bVisible) {
                        FTransform FinalTransform = Instance.CurrentRelativeTransform * BaseTransform;
                        Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, FinalTransform, true, true);
                    } else {
                        // Bei Unsichtbarkeit (z.B. außerhalb des Viewports) wird das ISM an den Nullpunkt verschoben.
                        Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true);
                    }
                }
            }
        }
    }));
}
