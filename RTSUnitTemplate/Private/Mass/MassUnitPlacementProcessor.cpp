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
    EntityQuery.AddRequirement<FMassUnitVisualFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::Optional);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::Optional);
    EntityQuery.AddTagRequirement<FMassUseSkeletalMovementTag>(EMassFragmentPresence::None);
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

    EntityQuery.ForEachEntityChunk(Context, ([&BatchedUpdates](FMassExecutionContext& Context) {
        TArrayView<FMassUnitVisualFragment> VisualList = Context.GetMutableFragmentView<FMassUnitVisualFragment>();
        TConstArrayView<FMassVisualEffectFragment> EffectList = Context.GetFragmentView<FMassVisualEffectFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        TConstArrayView<FMassRepresentationLODFragment> LODFragments = Context.GetFragmentView<FMassRepresentationLODFragment>();

        const bool bHasEffect = !EffectList.IsEmpty();
        const bool bHasVisibility = !VisibilityList.IsEmpty();
        const bool bHasLOD = !LODFragments.IsEmpty();

        const bool bChunkIsStopped = Context.DoesArchetypeHaveTag<FMassStateStopMovementTag>();
        const bool bChunkIsDead = Context.DoesArchetypeHaveTag<FMassStateDeadTag>();

            
        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassUnitVisualFragment& VisualFrag = VisualList[i];
            FMassAgentCharacteristicsFragment& CharFrag = CharList[i];

            bool bForceHidden = false;
            if (bHasEffect)
            {
                bForceHidden = EffectList[i].bForceHidden;
            }


            // LOD-Skip: Entities with LOD::Off get their ISMs hidden and are skipped
            if (bHasLOD && LODFragments[i].LOD == EMassLOD::Off)
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

            // Wunsch Punkt 2: Keine Casts mehr, direkt PositionedTransform (BaseTransform)
            FTransform BaseTransform = CharFrag.PositionedTransform;

            if (bChunkIsStopped && bChunkIsDead)
            {
                // Todes-Rotation nur für fliegende Einheiten fortführen
                if (CharFrag.bIsFlying)
                {
                    FVector CurrentLocation = BaseTransform.GetLocation();
                    FRotator CurrentRot = BaseTransform.GetRotation().Rotator();
                    
                    // Ziel-Z ist der Boden (LastGroundLocation + Kapsel-Offset)
                    float TargetZ = CharFrag.LastGroundLocation + CharFrag.CapsuleHeight;
                    bool bIsStillFalling = CurrentLocation.Z > (TargetZ + 1.f);

                    if (bIsStillFalling)
                    {
                        float DeltaTime = Context.GetDeltaTimeSeconds();

                        // A: Rotation (Spin) fortführen
                        if (CharFrag.VerticalDeathRotationMultiplier > 0.f)
                        {
                            CurrentRot.Yaw += CharFrag.VerticalDeathRotationMultiplier * DeltaTime;
                        }

                        // B: Sinken (Z-Bewegung) erzwingen
                        // 500.f dient als Beispiel für die Sinkgeschwindigkeit (entspricht ca. SyncProcessor-Speed)
                        float NewZ = FMath::FInterpConstantTo(CurrentLocation.Z, TargetZ, DeltaTime, 500.f);
                        CurrentLocation.Z = NewZ;

                        // C: Transformation aktualisieren
                        BaseTransform.SetLocation(CurrentLocation);
                        BaseTransform.SetRotation(FRotator(0.f, CurrentRot.Yaw, 0.f).Quaternion());

                        // D: Persistenz (Schreiben ins Fragment), damit der nächste Frame auf der neuen Position aufsetzt
                        CharFrag.PositionedTransform = BaseTransform;

                        // Update im nächsten Frame erzwingen, solange wir fallen/drehen
                        CharFrag.bTransformDirty = true;
                    }
                    else
                    {
                        // Am Boden angekommen: Rotation nivellieren (Pitch/Roll nullen)
                        BaseTransform.SetRotation(FRotator(0.f, CurrentRot.Yaw, 0.f).Quaternion());
                        CharFrag.PositionedTransform.SetRotation(BaseTransform.GetRotation());
                    }
                }
                else 
                {
                    // Nicht-fliegende tote Einheiten: Nur Rotation nivellieren
                    FRotator CurrentRot = BaseTransform.GetRotation().Rotator();
                    BaseTransform.SetRotation(FRotator(0.f, CurrentRot.Yaw, 0.f).Quaternion());
                }
            }

            bool bVisible = true;
            if (bHasVisibility)
            {
                const FMassVisibilityFragment& Vis = VisibilityList[i];
                bVisible = Vis.bIsVisibleEnemy && Vis.bIsOnViewport && !bForceHidden;
            }
            else
            {
                bVisible = !bForceHidden;
            }

            // Check if visibility state has changed to force an update
            bool bVisibilityChanged = false;
            for (const FMassUnitVisualInstance& Instance : VisualFrag.VisualInstances)
            {
                if (Instance.bWasVisible != bVisible)
                {
                    bVisibilityChanged = true;
                    break;
                }
            }

            // Dirty-Flag check: skip entities whose transform hasn't changed and whose visibility is unchanged
            if (!CharFrag.bTransformDirty && !VisualFrag.bUseSkeletalMovement && !bVisibilityChanged)
            {
                continue;
            }

            if (bVisibilityChanged)
            {
                CharFrag.bTransformDirty = true;
            }

            // Reset dirty flag after processing
            CharFrag.bTransformDirty = false;

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
