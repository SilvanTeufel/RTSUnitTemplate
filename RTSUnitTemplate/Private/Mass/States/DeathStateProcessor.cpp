// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/DeathStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"  
#include "MassCommonFragments.h" // Für FMassRepresentationFragment (optional für Sichtbarkeit)

// Für Actor Cast und Destroy
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Actors/EffectArea.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Math/RandomStream.h"
#include "Subsystems/UnitVisualManager.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Mass/MassActorBindingComponent.h"
#include "Hud/HUDBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"

UDeathStateProcessor::UDeathStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    // Reads fragments of other entities (9 sites) while units are dying, i.e. exactly while
    // archetypes are being moved. Must not run on a worker thread. See CurrentArchetype assert.
    bRequiresGameThreadExecution = true;
}

void UDeathStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::All); // Nur tote Entitäten
    
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Anhalten
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this);
}

void UDeathStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());

    if (SignalSubsystem)
    {
        RemoveDeadUnitSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RemoveDeadUnit)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UDeathStateProcessor, HandleRemoveDeadUnit));

        HideUnitSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::HideUnit)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UDeathStateProcessor, HandleHideUnit));

        SwitchToRuinSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SwitchToRuin)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UDeathStateProcessor, HandleSwitchToRuin));

        UpdateDissolveSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateDissolve)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UDeathStateProcessor, HandleUpdateDissolve));
    }
}

void UDeathStateProcessor::BeginDestroy()
{
    if (SignalSubsystem)
    {
        if (RemoveDeadUnitSignalDelegateHandle.IsValid())
        {
            auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RemoveDeadUnit);
            Delegate.Remove(RemoveDeadUnitSignalDelegateHandle);
            RemoveDeadUnitSignalDelegateHandle.Reset();
        }

        if (HideUnitSignalDelegateHandle.IsValid())
        {
            auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::HideUnit);
            Delegate.Remove(HideUnitSignalDelegateHandle);
            HideUnitSignalDelegateHandle.Reset();
        }

        if (SwitchToRuinSignalDelegateHandle.IsValid())
        {
            auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SwitchToRuin);
            Delegate.Remove(SwitchToRuinSignalDelegateHandle);
            SwitchToRuinSignalDelegateHandle.Reset();
        }

        if (UpdateDissolveSignalDelegateHandle.IsValid())
        {
            auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateDissolve);
            Delegate.Remove(UpdateDissolveSignalDelegateHandle);
            UpdateDissolveSignalDelegateHandle.Reset();
        }
    }
    Super::BeginDestroy();
}

void UDeathStateProcessor::HandleRemoveDeadUnit(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;

    TArray<FMassEntityHandle> EntitiesCopy = Entities; 

    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]()
    {
        if (!EntitySubsystem || !GetWorld()) return;

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        AControllerBase* PC = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
        if (!PC || !PC->HUDBase) return;
        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityActive(Entity)) continue;

            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
            if (ActorFragPtr && IsValid(ActorFragPtr->GetMutable()))
            {
                AUnitBase* UnitBase = Cast<AUnitBase>(ActorFragPtr->GetMutable());
                if (UnitBase)
                {
                    UnitBase->SetDeselected();
                    // CanBeSelected is a replicated property -> only the server may write it.
                    // If a client writes it here (e.g. after a transient false-death) the server
                    // never re-replicates the unchanged true value, leaving the unit permanently
                    // unselectable on that client. Let replication be the single source of truth.
                    if (UnitBase->HasAuthority())
                    {
                        UnitBase->CanBeSelected = false;
                    }
                    UnitBase->OpenHealthWidget = false;
                    UnitBase->bShowLevelOnly = false;
             
                    PC->RemoveUnitFromSelection(UnitBase);
                }
            }
        }
    });
}

void UDeathStateProcessor::HandleHideUnit(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;

    TArray<FMassEntityHandle> EntitiesCopy = Entities;

    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]()
    {
        if (!EntitySubsystem || !GetWorld()) return;

        UUnitVisualManager* VisualManager = GetWorld()->GetSubsystem<UUnitVisualManager>();

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityActive(Entity)) continue;

            if (VisualManager)
            {
                VisualManager->SetUnitVisualVisible(Entity, false);
            }

            if (FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
            {
                if (AActor* Actor = ActorFragPtr->GetMutable())
                {
                    if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
                    {
                        UE_LOG(LogTemp, Verbose,
                            TEXT("[DeathProc/HideUnit] %s NetMode=%d -> SetDeathVisualState(true) (Mesh wird versteckt)"),
                            *Unit->GetName(), (int)Unit->GetNetMode());
                        Unit->SetDeathVisualState(true);
                    }
                    else if (AEffectArea* Area = Cast<AEffectArea>(Actor))
                    {
                        bool bIsScalingActive = false;
                        if (FEffectAreaImpactFragment* Impact = EntityManager.GetFragmentDataPtr<FEffectAreaImpactFragment>(Entity))
                        {
                            // If it's supposed to scale on impact/death, and hasn't finished yet (VFX triggered means finished)
                            if (Impact->bScaleOnImpact && !Impact->bImpactVFXTriggered)
                            {
                                bIsScalingActive = true;
                            }
                        }

                        if (!bIsScalingActive)
                        {
                            Area->SetDeathVisualState(true);
                            if (FEffectAreaImpactFragment* Impact = EntityManager.GetFragmentDataPtr<FEffectAreaImpactFragment>(Entity))
                            {
                                Impact->bHasHiddenVisual = true;
                            }
                        }
                    }
                    else
                    {
                        Actor->SetActorHiddenInGame(true);
                    }
                }
            }
        }
    });
}

void UDeathStateProcessor::HandleSwitchToRuin(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;

    TArray<FMassEntityHandle> EntitiesCopy = Entities;

    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]()
    {
        if (!EntitySubsystem || !GetWorld()) return;

        UUnitVisualManager* VisualManager = GetWorld()->GetSubsystem<UUnitVisualManager>();
        if (!VisualManager) return;

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityActive(Entity)) continue;

            // The fragment flag is the single idempotency guard AND the level-trigger stop condition.
            FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
            if (!CharFrag || CharFrag->bRuinApplied) continue;

            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
            AMassUnitBase* Unit = ActorFragPtr ? Cast<AMassUnitBase>(ActorFragPtr->GetMutable()) : nullptr;

            // Ruin uses the pooled ISM path — skeletal-driven visuals are force-hidden by the placement
            // processor, so they can't host a ruin instance (buildings are ISM: bUseSkeletalMovement=false).
            // PERMANENT skips: mark applied so the level-triggered signal stops re-firing.
            if (!Unit || Unit->bUseSkeletalMovement)
            {
                CharFrag->bRuinApplied = true;
                continue;
            }

            UMassActorBindingComponent* Binding = Unit->MassActorBindingComponent;
            if (!Binding || !Binding->bSpawnRuinOnDeath || Binding->RuinMeshArray.Num() == 0)
            {
                CharFrag->bRuinApplied = true;
                continue;
            }

            // Deterministic pick seeded by the replicated UnitIndex so every machine agrees. If the index
            // hasn't replicated yet (INDEX_NONE), leave bRuinApplied false so the signal retries next tick
            // instead of the client missing the ruin forever.
            const int32 Seed = Unit->UnitIndex;
            if (Seed < 0) continue;

            FRandomStream Stream(Seed);
            const int32 PickIdx = Stream.RandRange(0, Binding->RuinMeshArray.Num() - 1);
            const float YawDegrees = Stream.FRandRange(0.f, 360.f);

            const FRuinMeshEntry& Entry = Binding->RuinMeshArray[PickIdx];
            if (!Entry.Mesh)
            {
                CharFrag->bRuinApplied = true; // configured but unusable — don't spin forever
                continue;
            }

            VisualManager->SwapUnitVisualToRuin(
                Entity, Unit, Entry.Mesh, Binding->RuinMaterialOverride,
                Binding->bRuinCastShadow, Entry.StretchFactor, YawDegrees);

            CharFrag->bRuinApplied = true;
        }
    });
}

void UDeathStateProcessor::HandleUpdateDissolve(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;

    TArray<FMassEntityHandle> EntitiesCopy = Entities;

    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]()
    {
        if (!EntitySubsystem || !GetWorld()) return;

        UUnitVisualManager* VisualManager = GetWorld()->GetSubsystem<UUnitVisualManager>();

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityActive(Entity)) continue;

            FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
            if (!CharFrag || CharFrag->DissolveDuration <= KINDA_SMALL_NUMBER) continue; // feature disabled

            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
            AMassUnitBase* Unit = ActorFragPtr ? Cast<AMassUnitBase>(ActorFragPtr->GetMutable()) : nullptr;
            if (!Unit) continue;

            UMassActorBindingComponent* Binding = Unit->MassActorBindingComponent;
            if (!Binding || !Binding->DissolveMaterial)
            {
                CharFrag->bDissolveApplied = true; // nothing to apply — don't keep retrying the swap
                continue;
            }

            // Fade amount 0..1 from time since death (StateTimer). Clamped so it holds at 1 after the window.
            const FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
            const float TimeSinceDeath = StateFrag ? StateFrag->StateTimer : 0.f;
            const float Alpha = FMath::Clamp(
                (TimeSinceDeath - CharFrag->DissolveStartTime) / FMath::Max(CharFrag->DissolveDuration, KINDA_SMALL_NUMBER),
                0.f, 1.f);

            // Blob-Schatten (MaterialDrivenShadows) mit dem BEGINN der Aufloesung abschalten und
            // nicht erst beim Ausblenden ueber HideActorTime: sonst liegt der Schattenfleck in
            // voller Staerke unter einer Leiche, die sich schon sichtbar aufloest.
            //
            // Genau einmal je Einheit: bDissolveApplied ist beim ersten Durchlauf noch false und
            // wird in beiden Zweigen unten gesetzt. Ohne die Sperre liefe je Takt und Leiche ein
            // ProcessEvent. AUnitBase::SetDeathVisualState schaltet den Schatten spaeter erneut ab
            // - derselbe Aufruf mit demselben Wert, das stoert nicht und bleibt der Rueckfall fuer
            // Einheiten ohne Aufloesung.
            // Der Zeiger ist ein AMassUnitBase; SetzeBlobSchattenAktiv sitzt eine Ebene tiefer
            // in APerformanceUnit, deshalb der Cast.
            if (!CharFrag->bDissolveApplied)
            {
                if (APerformanceUnit* LeistungsEinheit = Cast<APerformanceUnit>(Unit))
                {
                    LeistungsEinheit->SetzeBlobSchattenAktiv(false);
                }
            }

            if (Unit->bUseSkeletalMovement)
            {
                // SKM: swap the skeletal mesh's materials to dynamic instances of DissolveMaterial (once)
                // and drive the fade through the scalar parameter "Dissolve" each tick.
                USkeletalMeshComponent* Mesh = Unit->GetMesh();
                if (!Mesh) { CharFrag->bDissolveApplied = true; continue; }

                const int32 NumMats = Mesh->GetNumMaterials();
                for (int32 Slot = 0; Slot < NumMats; ++Slot)
                {
                    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
                    if (!MID || MID->Parent != Binding->DissolveMaterial)
                    {
                        MID = Mesh->CreateDynamicMaterialInstance(Slot, Binding->DissolveMaterial);
                    }
                    if (MID)
                    {
                        MID->SetScalarParameterValue(TEXT("Dissolve"), Alpha);
                    }
                }
                CharFrag->bDissolveApplied = true;
            }
            else if (VisualManager)
            {
                // ISM: NICHT mehr das Material tauschen, nur den Aufloeseanteil auf
                // PerInstanceCustomData 13 schreiben.
                //
                // Frueher wurde die Instanz in einen zweiten ISM-Pool umgehaengt (gleiches Mesh,
                // aber DissolveMaterial). Ein Materialwechsel heisst hier aber: NEUE Instanz in
                // einem ANDEREN ISM - und deren Custom Data 1..12 sind frisch, also Clipwert 0
                // und Frames 0..0. Genau daran sprang jede Leiche in die Bindepose, statt auf der
                // letzten Frame der Todesanimation zu verharren (der Todesclip ist PlayOnce).
                // Zugleich brachte das getauschte Material keine Vertexanimation mit.
                //
                // Seit Mat_Master_Skin_VAT die Funktion MF_XenoDissolve_AH in seine Opacity Mask
                // zieht, kann das VAT-Material den Aufloeseanteil selbst - die Instanz bleibt also,
                // wo sie ist, behaelt ihre Animationsdaten und loest sich trotzdem auf.
                CharFrag->bDissolveApplied = true;
                VisualManager->SetUnitDissolve(Entity, Alpha);
            }
        }
    });
}

void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UDeathStateProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UDeathStateProcessor);

    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UDeathStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto AgentFragList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAgentCharacteristicsFragment CharacteristicsFragment = AgentFragList[i];

            const float PrevTimer = StateFrag.StateTimer;
            StateFrag.StateTimer += ExecutionInterval;
            
            if (PrevTimer <= KINDA_SMALL_NUMBER)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[DeathProc/Client] Entity[%d:%d] ist als TOT in DeathStateProcessor angekommen -> StartDead/RemoveDeadUnit (HideActorTime=%.2f)"),
                    Entity.Index, Entity.SerialNumber, CharacteristicsFragment.HideActorTime);

                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::StartDead, Entity);
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::RemoveDeadUnit, Entity);

                if (CharacteristicsFragment.HideActorTime <= KINDA_SMALL_NUMBER)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::HideUnit, Entity);
                }
            }

            if (PrevTimer < CharacteristicsFragment.HideActorTime && StateFrag.StateTimer >= CharacteristicsFragment.HideActorTime)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::HideUnit, Entity);
            }

            // Ruin swap (after the hide). LEVEL-triggered while past SwitchToRuinMeshTime and not yet
            // applied, so a client that hasn't received the replicated UnitIndex (seed) yet retries until
            // it can; the fragment flag bRuinApplied stops it once done. Fired on the client too so the
            // ruin appears on every machine (handler runs on both).
            if (CharacteristicsFragment.SwitchToRuinMeshTime > KINDA_SMALL_NUMBER &&
                !CharacteristicsFragment.bRuinApplied &&
                StateFrag.StateTimer >= CharacteristicsFragment.SwitchToRuinMeshTime)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SwitchToRuin, Entity);
            }

            // Corpse dissolve (optional; SKM + ISM). Level-triggered every tick across the fade window so the
            // handler applies the DissolveMaterial once and advances the fade each tick. Fired on client AND
            // server (the handler runs on both). DissolveDuration is 0 unless a DissolveMaterial is set.
            if (CharacteristicsFragment.DissolveDuration > KINDA_SMALL_NUMBER &&
                StateFrag.StateTimer >= CharacteristicsFragment.DissolveStartTime &&
                StateFrag.StateTimer <= CharacteristicsFragment.DissolveStartTime + CharacteristicsFragment.DissolveDuration + ExecutionInterval)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UpdateDissolve, Entity);
            }
        }
    });
}

void UDeathStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto AgentFragList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAgentCharacteristicsFragment CharacteristicsFragment = AgentFragList[i];

            Velocity.Value = FVector::ZeroVector;

            const float PrevTimer = StateFrag.StateTimer;
            StateFrag.StateTimer += ExecutionInterval;
    
            if (PrevTimer <= KINDA_SMALL_NUMBER)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::StartDead, Entity);
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::RemoveDeadUnit, Entity);

                if (CharacteristicsFragment.HideActorTime <= KINDA_SMALL_NUMBER)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::HideUnit, Entity);
                }
            }

            if (PrevTimer < CharacteristicsFragment.HideActorTime && StateFrag.StateTimer >= CharacteristicsFragment.HideActorTime)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::HideUnit, Entity);
            }

            // Ruin swap (after the hide, before despawn). LEVEL-triggered while past SwitchToRuinMeshTime
            // and not yet applied (see client path). Fired on the server (+ standalone/listen host).
            if (CharacteristicsFragment.SwitchToRuinMeshTime > KINDA_SMALL_NUMBER &&
                !CharacteristicsFragment.bRuinApplied &&
                StateFrag.StateTimer >= CharacteristicsFragment.SwitchToRuinMeshTime)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SwitchToRuin, Entity);
            }

            // Corpse dissolve (optional; SKM + ISM). Level-triggered every tick across the fade window so the
            // handler applies the DissolveMaterial once and advances the fade each tick. Fired on server AND
            // client (the handler runs on both). DissolveDuration is 0 unless a DissolveMaterial is set.
            if (CharacteristicsFragment.DissolveDuration > KINDA_SMALL_NUMBER &&
                StateFrag.StateTimer >= CharacteristicsFragment.DissolveStartTime &&
                StateFrag.StateTimer <= CharacteristicsFragment.DissolveStartTime + CharacteristicsFragment.DissolveDuration + ExecutionInterval)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::UpdateDissolve, Entity);
            }

            if (StateFrag.StateTimer >= CharacteristicsFragment.DespawnTime+1.f)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::EndDead, Entity);
            }
        }
    });
}
