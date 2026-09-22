// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassUnitPlacementProcessor.h"
#include "MassCommonFragments.h"
#include "Characters/Unit/BuildingBase.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "MassRepresentationFragments.h"
#include "GameFramework/Actor.h"
#include "Characters/Unit/UnitBase.h"
#include "ProfilingDebugging/CsvProfiler.h"

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
    // Optional, damit kein Entity aus der Abfrage faellt, dem das Fragment fehlt.
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    // Fuer den Abgleich unten: nur damit ein stehendes GEBAEUDE erkannt wird, das den Stop-Tag
    // nie bekommen hat. Gemessen am 22.09.2026: WallTower-Extensions tragen ihn nicht.
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UMassUnitPlacementProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UMassUnitPlacementProcessor);

    // Batch-Update: collect updates per ISM component instead of calling UpdateInstanceTransform individually
    TMap<UInstancedStaticMeshComponent*, TArray<FISMInstanceUpdate>> BatchedUpdates;

    EntityQuery.ForEachEntityChunk(Context, ([&BatchedUpdates](FMassExecutionContext& Context) {
        TArrayView<FMassUnitVisualFragment> VisualList = Context.GetMutableFragmentView<FMassUnitVisualFragment>();
        TConstArrayView<FMassVisualEffectFragment> EffectList = Context.GetFragmentView<FMassVisualEffectFragment>();
        TConstArrayView<FMassVisibilityFragment> VisibilityList = Context.GetFragmentView<FMassVisibilityFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        TConstArrayView<FMassRepresentationLODFragment> LODFragments = Context.GetFragmentView<FMassRepresentationLODFragment>();
        TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
        TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();

        const bool bHasEffect = !EffectList.IsEmpty();
        const bool bHasVisibility = !VisibilityList.IsEmpty();
        const bool bHasLOD = !LODFragments.IsEmpty();

        const bool bChunkIsStopped = Context.DoesArchetypeHaveTag<FMassStateStopMovementTag>();
        const bool bChunkIsDead = Context.DoesArchetypeHaveTag<FMassStateDeadTag>();

            
        for (int i = 0; i < Context.GetNumEntities(); ++i) {
            FMassUnitVisualFragment& VisualFrag = VisualList[i];
            FMassAgentCharacteristicsFragment& CharFrag = CharList[i];

            // DAS FTransformFragment EINER STEHENDEN EINHEIT NACHFUEHREN.
            //
            // GEMESSEN am 22.09.2026 auf Level_6_Survive: bei Gebaeuden laeuft PositionedTransform
            // korrekt mit (daraus wird die ISM gezeichnet, und die liegt sichtbar richtig auf dem
            // Gelaende), waehrend das FTransformFragment auf einem alten Wert STEHENBLEIBT -
            // Xeno-Gebaeude auf konstant 11.0 bei Boden 7.0, WallTower auf dem blanken Boden
            // (Abweichung -199.0 = genau die Kapselhalbhoehe) oder auf eingefrorenen
            // Zwischenwerten aus dem Landeanflug (-42.2, -231.1, -320.9, -470.4).
            //
            // Es schreibt nicht etwa jemand einen falschen Wert: es schreibt NIEMAND. Belegt durch
            // zwei Messungen mit null Treffern - [BodenZweig] (HandleGroundAndHeight laeuft fuer
            // stehende Gebaeude gar nicht) und [TFragSchreiber] (weder LookAt noch der
            // Bewegungsprozessor fassen sie an), waehrend im selben Lauf 4 Gebaeude abwichen.
            //
            // GetMassActorLocation liest das FTransformFragment ZUERST. Dadurch schlug der alte
            // Wert auf alles durch, was die Mass-Position liest: Vorschauflaeche der Extension,
            // Hoverpunkt, EnergyWall-Sockel, BuildArea auf Highground.
            //
            // NUR fuer angehaltene Einheiten. Bei einer bewegten ist das FTransformFragment der
            // fuehrende Speicher und PositionedTransform folgt ihm - dort waere dieser Abgleich
            // genau verkehrt herum und wuerde die Bewegung zurueckwerfen.
            // Ein GEBAEUDE, das nicht mehr fliegt und nicht mehr fahren darf, steht - auch ohne
            // Stop-Tag. Die WallTower-Extensions bekommen ihn nie; in der Messung vom 22.09.2026
            // blieben deshalb 23 von 47 Gebaeuden auf dem alten Wert stehen, die Tuerme auf genau
            // -199.0 (eine Kapselhalbhoehe). Waehrend des Landeanflugs ist CanMove noch wahr, dort
            // fuehrt das TransformFragment - genau deshalb wird hier auf CanMove geprueft und
            // nicht bloss auf "ist ein Gebaeude".
            bool bIsSettledBuilding = false;
            if (!ActorList.IsEmpty())
            {
                if (const AUnitBase* OwnerUnit = Cast<AUnitBase>(ActorList[i].Get()))
                {
                    bIsSettledBuilding = OwnerUnit->IsA(ABuildingBase::StaticClass())
                                      && !OwnerUnit->CanMove
                                      && !CharFrag.bIsFlying;
                }
            }

            if ((bChunkIsStopped || bIsSettledBuilding) && !TransformList.IsEmpty())
            {
                FTransform& LiveTransform = TransformList[i].GetMutableTransform();
                if (!LiveTransform.GetLocation().Equals(CharFrag.PositionedTransform.GetLocation(), 1.f))
                {
                    LiveTransform.SetLocation(CharFrag.PositionedTransform.GetLocation());
                }
            }

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
