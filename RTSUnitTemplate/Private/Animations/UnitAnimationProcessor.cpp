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
#include "MassMovementFragments.h"   // FMassVelocityFragment - diagnostic: wanted vs. actual speed

// All ISM animation custom data lives in indices 1..12 (see the *CustomDataIndex members in
// UnitAnimationProcessor.h), so every animated ISM needs at least this many custom-data floats.
// The ISM is pre-sized ONCE at creation (UUnitVisualManager::GetOrCreateISM /
// UUnitVisualManager::AssignUnitVisual / the AMassUnitBase constructor) and is NEVER resized here:
// on a pooled/shared ISM, SetNumCustomDataFloats reallocates and zero-fills the custom data of
// EVERY instance, which would wipe other units' animation state mid-play.
// NOTE: the three creation sites size to 14 floats — indices 1..12 animation + index 13 corpse-dissolve
// (UUnitVisualManager::SetUnitDissolve). This constant is only the animation MINIMUM (the guard below is
// >=), so it stays 13; if you change any *CustomDataIndex value, update this constant and those sites.
static constexpr int32 RequiredCustomDataFloats = 13;

// Muss mit dem TimeMultiplier in Mat_Master_Skin_VAT uebereinstimmen: das Material rechnet
// Frames damit in Sekunden um. Wird die Zahl dort geaendert, muss sie hier nachgezogen werden.
static constexpr float VATBildrate = 30.0f;

// Returns the row matching State; if none exists, falls back to the Idle row; nullptr if neither is
// present. This guarantees an entity entering a state with no table row still receives valid custom
// data instead of keeping stale/zero values (which freezes the vertex animation).
static const FISMAnimationData* FindISMRowForStateOrIdle(UDataTable* Table, TEnumAsByte<UnitData::EState> State,
    bool* bOutIdleFallback = nullptr)
{
    if (bOutIdleFallback)
    {
        *bOutIdleFallback = false;
    }
    if (!Table)
    {
        return nullptr;
    }

    const FISMAnimationData* ExactMatch = nullptr;
    const FISMAnimationData* IdleMatch = nullptr;
    for (const TPair<FName, uint8*>& It : Table->GetRowMap())
    {
        if (const FISMAnimationData* Row = reinterpret_cast<const FISMAnimationData*>(It.Value))
        {
            if (Row->AnimState == State)
            {
                ExactMatch = Row;
                break;
            }
            if (Row->AnimState == UnitData::Idle)
            {
                IdleMatch = Row;
            }
        }
    }

    if (!ExactMatch && IdleMatch && bOutIdleFallback)
    {
        // Der Ruecktritt auf Idle ist der Grund, warum eine Einheit ploetzlich wieder in der
        // Leerlaufpose steht, obwohl sie tot ist oder etwas anderes tut: fuer ihren Zustand gibt
        // es keine Zeile, also greift die Leerlaufzeile. Ohne diese Meldung sieht man nur das
        // Ergebnis und raet ueber die Ursache.
        *bOutIdleFallback = true;
    }

    return ExactMatch ? ExactMatch : IdleMatch;
}

// Skeletal counterpart of FindISMRowForStateOrIdle above: exact row, else the Idle row, else nullptr.
// The skeletal path used to inline a plain search loop and write NOTHING when no row matched, while
// still latching LastProcessedState to the new state. The blend points therefore kept the PREVIOUS
// state's values and were never retried, so the unit went on playing the old animation - a unit that
// left Run for a state with no row (Rooted and ContinousAttack have no row in either shipped
// DT_UnitAnimData) kept running on the spot. The ISM path has carried this fallback and the comment
// explaining why since it was written; the skeletal path was simply missed.
static const FUnitAnimData* FindAnimRowForStateOrIdle(UDataTable* Table, TEnumAsByte<UnitData::EState> State, bool& bOutUsedFallback)
{
    bOutUsedFallback = false;
    if (!Table)
    {
        return nullptr;
    }

    const FUnitAnimData* ExactMatch = nullptr;
    const FUnitAnimData* IdleMatch = nullptr;
    for (const TPair<FName, uint8*>& It : Table->GetRowMap())
    {
        if (const FUnitAnimData* Row = reinterpret_cast<const FUnitAnimData*>(It.Value))
        {
            if (Row->AnimState == State)
            {
                ExactMatch = Row;
                break;
            }
            if (Row->AnimState == UnitData::Idle)
            {
                IdleMatch = Row;
            }
        }
    }

    bOutUsedFallback = (ExactMatch == nullptr && IdleMatch != nullptr);
    return ExactMatch ? ExactMatch : IdleMatch;
}

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

    // Diagnostic only, therefore Optional: a missing fragment must never filter an entity out of
    // the animation update itself.
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

    // Gate on the dedicated StopAnimation tag (mirrors StopMovement except for CanAnimate opt-ins),
    // NOT StopMovement itself — this is what lets a stationary building with CanAnimate=true animate.
    EntityQuery.AddTagRequirement<FMassStateStopAnimationTag>(EMassFragmentPresence::None);
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
        const FTransformFragment* TransformList = ChunkContext.GetFragmentView<FTransformFragment>().GetData();
        const FMassVelocityFragment* VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>().GetData();
        const float ChunkDeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
        {
            const FMassUnitVisualFragment& VisualFrag = VisualList[i];
            FUnitAnimationFragment& AnimFrag = AnimList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            
            if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorList[i].GetMutable()))
            {
                const TEnumAsByte<UnitData::EState> RealState = UnitBase->GetUnitState();

                // ---- Standing still while a movement state is active ------------------------
                //
                // Run/Chase/PatrolRandom and the GoTo states stay active while a unit does not
                // move: the path search is running, the destination is reached, or the way is
                // blocked. The animation then showed running on the spot.
                //
                // Deliberately decided HERE and not in UUnitBaseAnimInstance, where the same
                // correction already existed: that one only ever ran for units the local camera
                // could see (IsOnViewport) and it only rewrote CharAnimState - the blend points,
                // which is what the ISM and vertex-animation path actually plays, kept the
                // movement row. Doing it at the source covers every representation.
                //
                // Only movement states are listed. Standing still is the correct picture for
                // Attack, Pause, Build, ResourceExtraction, Casting and Idle, so those keep their
                // own row and are never touched.
                //
                // Measured from the ACTUAL displacement, not from the velocity fragment: the
                // fragment carries what the movement WANTS, which is exactly the value in doubt.
                const bool bMovementState =
                       RealState == UnitData::Run
                    || RealState == UnitData::Chase
                    || RealState == UnitData::Patrol
                    || RealState == UnitData::PatrolRandom
                    || RealState == UnitData::GoToBase
                    || RealState == UnitData::GoToBuild
                    || RealState == UnitData::GoToResourceExtraction;

                bool bStandingStill = false;
                float MeasuredSpeed = -1.f;
                {
                    const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
                    if (!bMovementState || !TransformList)
                    {
                        AnimStandWatches.Remove(Entity);
                    }
                    else
                    {
                        const FVector Location = TransformList[i].GetTransform().GetLocation();
                        FAnimStandWatch& Watch = AnimStandWatches.FindOrAdd(Entity);

                        if (!Watch.bHasLocation)
                        {
                            Watch.LastLocation = Location;
                            Watch.bHasLocation = true;
                        }
                        else
                        {
                            const float Moved = FVector::Dist2D(Watch.LastLocation, Location);
                            MeasuredSpeed = ChunkDeltaTime > KINDA_SMALL_NUMBER
                                ? Moved / ChunkDeltaTime : 0.f;
                            Watch.LastLocation = Location;

                            // Entering takes AnimStandMinSeconds, leaving takes a single moving
                            // frame. Asymmetric on purpose: a unit that starts walking has to look
                            // like it immediately, while a single blocked frame must not flip the
                            // animation.
                            if (MeasuredSpeed <= AnimStandSpeedThreshold)
                            {
                                Watch.SecondsStanding += ChunkDeltaTime;
                            }
                            else
                            {
                                Watch.SecondsStanding = 0.f;
                            }

                            bStandingStill = (Watch.SecondsStanding >= AnimStandMinSeconds);
                        }
                    }
                }

                const TEnumAsByte<UnitData::EState> CurrentState =
                    (bAnimStandFix && bStandingStill) ? TEnumAsByte<UnitData::EState>(UnitData::Idle) : RealState;

                if (bAnimStandDiagnostics && bMovementState)
                {
                    ++AnimStandObserved;
                    if (VisualFrag.bUseSkeletalMovement) ++AnimStandObservedSkeletal;

                    if (bStandingStill)
                    {
                        ++AnimStandCount;
                        if (VisualFrag.bUseSkeletalMovement) ++AnimStandSkeletal;
                        if (UnitBase->IsOnViewport) ++AnimStandOnViewport;

                        // "No velocity fragment" and "fragment wants to move" are different causes
                        // and must not collapse into one number.
                        if (!VelocityList)
                        {
                            ++AnimStandNoVelocity;
                        }
                        else if (VelocityList[i].Value.Size2D() > AnimStandSpeedThreshold)
                        {
                            ++AnimStandWantsToMove;
                        }

                        if (CurrentWorldTime >= AnimStandNextDetailTime)
                        {
                            AnimStandNextDetailTime = CurrentWorldTime + 2.f;
                            UE_LOG(LogTemp, Warning,
                                TEXT("[LaufAufDerStelle] %s Zustand=%d -> Idle  gemessen=%.1f  soll=%.1f  skelettal=%d  imBild=%d"),
                                *UnitBase->GetName(), (int32)RealState.GetValue(), MeasuredSpeed,
                                VelocityList ? VelocityList[i].Value.Size2D() : -1.f,
                                VisualFrag.bUseSkeletalMovement ? 1 : 0,
                                UnitBase->IsOnViewport ? 1 : 0);
                        }
                    }
                }
                // ---- end standing-still handling --------------------------------------------

                // Zieht die Instanz auf eine ANDERE ISM um (der UUnitVisualManager holt sie von der
                // eigenen Komponente der Einheit auf eine gepoolte), stehen dort noch Nullen. Das Material
                // liest dann Frames 0..0 und die Einheit steht still, bis zufaellig ein Zustandswechsel
                // neu schreibt - genau das Standbild direkt nach dem Spawnen. Also den Wechsel erneut
                // anfordern, sobald sich Komponente oder Instanzindex geaendert haben.
                if (!VisualFrag.bUseSkeletalMovement && AnimFrag.ISMAnimationDataTable
                    && AnimFrag.LastWrittenInstanceIndex != INDEX_NONE
                    && VisualFrag.VisualInstances.Num() > 0)
                {
                    UInstancedStaticMeshComponent* AktuellesZiel = VisualFrag.VisualInstances[0].TargetISM.Get();
                    const int32 AktuellerIndex = VisualFrag.VisualInstances[0].InstanceIndex;

                    if (AktuellesZiel && AktuellerIndex != INDEX_NONE
                        && (AnimFrag.LastWrittenISM.Get() != AktuellesZiel
                            || AnimFrag.LastWrittenInstanceIndex != AktuellerIndex))
                    {
                        UE_LOG(LogTemp, Log,
                            TEXT("[ISMAnim] %s: Instanz umgezogen (%s#%d -> %s#%d) - Animationsdaten werden neu geschrieben."),
                            *UnitBase->GetName(),
                            *GetNameSafe(AnimFrag.LastWrittenISM.Get()), AnimFrag.LastWrittenInstanceIndex,
                            *GetNameSafe(AktuellesZiel), AktuellerIndex);

                        AnimFrag.LastProcessedState = UnitData::None;
                    }
                }

                // 1. Check for State Change and Update Targets
                if (AnimFrag.LastProcessedState != CurrentState)
                {
                    // Only latch LastProcessedState once the change has actually been applied. The
                    // skeletal and no-table paths always apply; the ISM path may defer (and retry next
                    // frame) if its target instance isn't ready, so it never latches a state whose
                    // custom data was never written.
                    bool bCommitted = true;

                    if (VisualFrag.bUseSkeletalMovement)
                    {
                        if (UUnitBaseAnimInstance* AnimInst = Cast<UUnitBaseAnimInstance>(UnitBase->GetMesh()->GetAnimInstance()))
                        {
                            // Exact row -> Idle fallback. Never leave the blend points on the previous
                            // state's values (see FindAnimRowForStateOrIdle).
                            bool bUsedIdleFallback = false;
                            if (const FUnitAnimData* RowData = FindAnimRowForStateOrIdle(AnimInst->AnimDataTable, CurrentState, bUsedIdleFallback))
                            {
                                AnimFrag.TargetBlendPoint_1 = RowData->BlendPoint_1;
                                AnimFrag.TargetBlendPoint_2 = RowData->BlendPoint_2;
                                AnimFrag.TransitionRate_1 = RowData->TransitionRate_1;
                                AnimFrag.TransitionRate_2 = RowData->TransitionRate_2;
                                AnimFrag.Resolution_1 = RowData->Resolution_1;
                                AnimFrag.Resolution_2 = RowData->Resolution_2;
                                AnimFrag.Sound = RowData->Sound;

                                if (bUsedIdleFallback)
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("[AnimRow] %s: no row for state %d in %s - fell back to Idle"),
                                        *UnitBase->GetName(), (int32)CurrentState.GetValue(), *GetNameSafe(AnimInst->AnimDataTable));
                                }
                            }
                        }
                    }
                    else if (AnimFrag.ISMAnimationDataTable)
                    {
                        // Resolve the target instance first; we only commit the state change once we
                        // can actually write to it.
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

                        // The ISM is pre-sized to RequiredCustomDataFloats at creation. We must NOT call
                        // SetNumCustomDataFloats here: on a pooled/shared ISM it reallocates and zero-fills
                        // EVERY instance's custom data, wiping other units' animation state mid-play. If the
                        // target is missing or (defensively) still undersized, defer and retry next frame
                        // instead of latching a state whose data never got written.
                        if (TargetISM && InstanceIndex != INDEX_NONE && TargetISM->NumCustomDataFloats >= RequiredCustomDataFloats)
                        {
                            // Exact row -> Idle fallback -> safe static pose (never leaves stale/zero data).
                            bool bIdleRuecktritt = false;
                            const FISMAnimationData* RowData = FindISMRowForStateOrIdle(
                                AnimFrag.ISMAnimationDataTable, CurrentState, &bIdleRuecktritt);
                            if (bIdleRuecktritt)
                            {
                                UE_LOG(LogTemp, Warning,
                                    TEXT("[ISMAnim] %s: keine Zeile fuer Zustand %d in %s - Ruecktritt auf Idle."),
                                    *UnitBase->GetName(), (int32)CurrentState.GetValue(),
                                    *GetNameSafe(AnimFrag.ISMAnimationDataTable));
                            }

                            // Mehrere Zustaende teilen sich denselben Clip: Run/Chase/Patrol laufen alle
                            // ueber die Lauf-Frames, Idle/Pause/PatrolIdle ueber die Leerlauf-Frames. Wechselt
                            // eine Einheit zwischen solchen Zustaenden, darf die Wiedergabe NICHT neu starten.
                            // Sonst wandert der laufende Stand in den Prev-Slot, der Current-Slot beginnt mit
                            // frischer Startzeit von vorn, und das Material blendet zwei phasenverschobene
                            // Kopien DERSELBEN Animation gegeneinander - im Bild ein Stocken mit Zuruecksetzen.
                            // Gleicher Clipwert und gleicher Bildbereich heisst: nur der Zustandsname hat sich
                            // geaendert, also lediglich den Zustand nachziehen.
                            // Abspielgeschwindigkeit des ANGRIFFSCLIPS aus der Angriffsdauer rechnen.
                            //
                            // Der Angriffszustand dauert AttackDuration Sekunden, danach schaltet der
                            // AttackStateProcessor auf Pause. Der Clip aus der Tabelle ist damit fast nie
                            // deckungsgleich: ist er kuerzer, haelt PlayOnce die letzte Frame und die Einheit
                            // steht still (beim MonsterFly 63 % der Angriffsdauer, beim SiegeColossus 77 %);
                            // ist er laenger, wird er mitten in der Bewegung abgeschnitten. Aus Bildzahl und
                            // Dauer laesst sich die passende Geschwindigkeit direkt ausrechnen - und sie
                            // bleibt richtig, wenn die Angriffsdauer spaeter geaendert wird.
                            //
                            // Nur fuer ISM/Vertex und nur fuer den Angriff: Leerlauf und Laufen loopen ohne
                            // feste Dauer, und der Todesclip soll seine letzte Frame ausdruecklich behalten.
                            // Der Skelettpfad macht dasselbe fuer ContinuousAttack schon weiter unten.
                            float EffektivePlayRate = RowData ? RowData->PlayRate : 1.0f;
                            if (RowData
                                && Stats.bPlayRateRunTimeCalculation
                                && CurrentState == UnitData::Attack
                                && Stats.AttackDuration > KINDA_SMALL_NUMBER)
                            {
                                const float Frames = RowData->EndFrame - RowData->StartFrame;
                                if (Frames > KINDA_SMALL_NUMBER)
                                {
                                    EffektivePlayRate = Frames / (VATBildrate * Stats.AttackDuration);
                                }
                            }

                            const bool bGleicherClip =
                                RowData
                                && AnimFrag.LastProcessedState != UnitData::None
                                && FMath::IsNearlyEqual(RowData->StateCustomDataValue, AnimFrag.TargetStateCustomDataValue)
                                && FMath::IsNearlyEqual(RowData->StartFrame, AnimFrag.CurrentStartFrame)
                                && FMath::IsNearlyEqual(RowData->EndFrame, AnimFrag.CurrentEndFrame)
                                && FMath::IsNearlyEqual(EffektivePlayRate, AnimFrag.CurrentPlayRate);

                            if (bGleicherClip)
                            {
                                // Nur die Uebergangsrate nachziehen. Startzeit, Prev-Slot und BlendAlpha
                                // bleiben unberuehrt, damit die laufende Animation ungestoert weiterlaeuft.
                                AnimFrag.TransitionRate_1 = RowData->TransitionRate;
                            }
                            else
                            {
                                AnimFrag.PrevTargetStateCustomDataValue = AnimFrag.TargetStateCustomDataValue;
                                AnimFrag.PrevStartTime = AnimFrag.CurrentStartTime;
                                AnimFrag.PrevStartFrame = AnimFrag.CurrentStartFrame;
                                AnimFrag.PrevEndFrame = AnimFrag.CurrentEndFrame;
                                AnimFrag.PrevPlayRate = AnimFrag.CurrentPlayRate;

                                if (RowData)
                                {
                                    AnimFrag.TargetStateCustomDataValue = RowData->StateCustomDataValue;
                                    AnimFrag.TransitionRate_1 = RowData->TransitionRate;
                                    AnimFrag.CurrentStartFrame = RowData->StartFrame;
                                    AnimFrag.CurrentEndFrame = RowData->EndFrame;
                                    AnimFrag.CurrentPlayRate = EffektivePlayRate;
                                }
                                else
                                {
                                    // No matching row and no Idle row: write an explicit, non-degenerate static
                                    // pose so the unit stays visible/animated instead of freezing on zero data.
                                    AnimFrag.TargetStateCustomDataValue = 0.0f;
                                    AnimFrag.TransitionRate_1 = 0.5f;
                                    AnimFrag.CurrentStartFrame = 0.0f;
                                    AnimFrag.CurrentEndFrame = 1.0f;
                                    AnimFrag.CurrentPlayRate = 1.0f;
                                }
                                AnimFrag.CurrentStartTime = CurrentWorldTime;
                                AnimFrag.BlendAlpha = 0.0f;

                                // Beim ALLERERSTEN Schreiben gibt es keine Vorgeschichte: die Prev-Werte
                                // stuenden auf 0 und BlendAlpha auf 0, das Material zeigte also zu 100% den
                                // Prev-Slot mit Frames 0..0 - ein Standbild. Ohne vorherige Animation gibt es
                                // nichts zu blenden, also den aktuellen Clip gleich voll aufblenden.
                                if (AnimFrag.LastProcessedState == UnitData::None)
                                {
                                    AnimFrag.PrevTargetStateCustomDataValue = AnimFrag.TargetStateCustomDataValue;
                                    AnimFrag.PrevStartTime = AnimFrag.CurrentStartTime;
                                    AnimFrag.PrevStartFrame = AnimFrag.CurrentStartFrame;
                                    AnimFrag.PrevEndFrame = AnimFrag.CurrentEndFrame;
                                    AnimFrag.PrevPlayRate = AnimFrag.CurrentPlayRate;
                                    AnimFrag.BlendAlpha = 1.0f;
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

                                AnimFrag.LastWrittenISM = TargetISM;
                                AnimFrag.LastWrittenInstanceIndex = InstanceIndex;
                            }

                            // Genau EINMAL je Einheit: der Uebergang von "noch nie geschrieben" auf den
                            // ersten echten Zustand. Bleibt diese Zeile aus, obwohl die Einheit im Spiel
                            // steht, wurden nie Instanzdaten geschrieben - die Einheit steht dann auf
                            // Frame 0 still, statt ihre Animation zu zeigen.
                            if (AnimFrag.LastProcessedState == UnitData::None)
                            {
                                UE_LOG(LogTemp, Log,
                                    TEXT("[ISMAnim] %s: erste Instanzdaten Zustand=%d Frames %.0f..%.0f Rate=%.2f Clip=%.0f"),
                                    *UnitBase->GetName(), (int32)CurrentState.GetValue(),
                                    AnimFrag.CurrentStartFrame, AnimFrag.CurrentEndFrame,
                                    AnimFrag.CurrentPlayRate, AnimFrag.TargetStateCustomDataValue);
                            }
                        }
                        else
                        {
                            // Target ISM not ready/undersized -> retry next frame (do not latch).
                            bCommitted = false;
                        }
                    }
                    else
                    {
                        // Die Animationstabelle haengt am Fragment erst nach der Mass-Registrierung
                        // (UMassActorBindingComponent::InitializeMassEntityStatsFromOwner). Tickt dieser
                        // Prozessor davor - beim Spawn und bei im Level platzierten Einheiten der Regelfall -,
                        // greift KEINER der beiden Zweige. Ohne das Folgende galte der Wechsel trotzdem als
                        // erledigt: der allererste Zustand (meist Idle) wuerde verschluckt, die Instanzdaten
                        // blieben auf 0 und die Einheit stuende auf Frame 0 still, bis sie zufaellig den
                        // Zustand wechselt. Deshalb die Tabelle hier nachholen und den Wechsel offen lassen.
                        if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(UnitBase))
                        {
                            AnimFrag.ISMAnimationDataTable = MassUnit->ISMAnimationDataTable;
                        }

                        // Nur zurueckstellen, wenn es ueberhaupt etwas zu schreiben gibt - eine Einheit
                        // ganz ohne Tabelle wuerde sonst in jedem Takt erneut in diesen Block laufen.
                        bCommitted = (AnimFrag.ISMAnimationDataTable == nullptr);

                        if (!bCommitted)
                        {
                            UE_LOG(LogTemp, Verbose,
                                TEXT("[ISMAnim] %s: Tabelle beim Zustandswechsel noch nicht am Fragment - nachgeholt, Wechsel bleibt offen."),
                                *UnitBase->GetName());
                        }
                    }

                    if (bCommitted)
                    {
                        AnimFrag.LastProcessedState = CurrentState;
                    }
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

    if (bAnimStandDiagnostics)
    {
        AnimStandReportTimer += Context.GetDeltaTimeSeconds();
        if (AnimStandReportTimer >= 20.f)
        {
            const float Share = AnimStandObserved > 0
                ? 100.f * float(AnimStandCount) / float(AnimStandObserved) : 0.f;
            UE_LOG(LogTemp, Warning,
                TEXT("[LaufAufDerStelle] %d von %d Bewegungszustaenden stehen still (%.2f%%)  davon skelettal=%d  soll>0=%d  ohneGeschwindigkeitsfragment=%d  IM BILD=%d   [beobachtet skelettal=%d von %d]"),
                AnimStandCount, AnimStandObserved, Share, AnimStandSkeletal, AnimStandWantsToMove,
                AnimStandNoVelocity, AnimStandOnViewport, AnimStandObservedSkeletal, AnimStandObserved);

            AnimStandReportTimer = 0.f;
            AnimStandObserved = 0;
            AnimStandObservedSkeletal = 0;
            AnimStandCount = 0;
            AnimStandSkeletal = 0;
            AnimStandWantsToMove = 0;
            AnimStandNoVelocity = 0;
            AnimStandOnViewport = 0;
        }
    }
}
