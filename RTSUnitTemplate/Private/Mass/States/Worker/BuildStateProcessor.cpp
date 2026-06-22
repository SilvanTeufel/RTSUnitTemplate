// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/BuildStateProcessor.h" // Adjust path

// Engine & Mass includes
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"     // For FMassSignalPayload and sending signals
#include "Async/Async.h"             // For AsyncTask
#include "Engine/World.h"            // For UWorld
#include "Templates/SubclassOf.h"    // For TSubclassOf check
#include "MassCommonFragments.h"

// Your project specific includes
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/ConstructionUnit.h"

// No Actor includes, no Movement includes needed

UBuildStateProcessor::UBuildStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UBuildStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Update StateTimer
    // Read-only fragments needed by the external system handling SpawnBuildingRequest signal:
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadWrite); // For BuildAreaPosition
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // For TeamID
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional); // Read replicated BuildArea to face it (server+client)

    // State Tag
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);

    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UBuildStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    
    if (SignalSubsystem)
    {
        CalculateConstructionScaleDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncConstructionScale).AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UBuildStateProcessor, CalculateConstructionScale));
    }
}

void UBuildStateProcessor::BeginDestroy()
{
    if (SignalSubsystem && CalculateConstructionScaleDelegateHandle.IsValid())
    {
        auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncConstructionScale);
        Delegate.Remove(CalculateConstructionScaleDelegateHandle);
    }
    Super::BeginDestroy();
}

void UBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    
    const UWorld* World = EntityManager.GetWorld();

    if (!World) { return; } // Early exit if world is invalid

    if (!SignalSubsystem) return;

    const bool bIsClient = (World->GetNetMode() == NM_Client);

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, bIsClient](FMassExecutionContext& ChunkContext)
    {
        auto AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto WorkerStatsList = ChunkContext.GetMutableFragmentView<FMassWorkerStatsFragment>();
        const int32 NumEntities = ChunkContext.GetNumEntities();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (bIsClient)
            {
                ClientExecute(EntityManager, ChunkContext, AIStateList[i], WorkerStatsList[i], ChunkContext.GetEntity(i), i);
            }
            else
            {
                ServerExecute(EntityManager, ChunkContext, AIStateList[i], WorkerStatsList[i], ChunkContext.GetEntity(i), i);
            }
        }
    });

}

void UBuildStateProcessor::ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    if (WorkerStats.BuildingAvailable || !WorkerStats.BuildingAreaAvailable)
    {
        AIState.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToBase, Entity);
        }
        return;
    }

    AIState.StateTimer += ExecutionInterval;
    AIState.DeltaTime = ExecutionInterval;

    if (SignalSubsystem)
    {
        SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SyncCastTime, Entity);
        // SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SyncConstructionScale, Entity);
    }

    // --- Completion Check ---
    if (AIState.StateTimer >= WorkerStats.BuildTime && !AIState.SwitchingState)
    {
        AIState.SwitchingState = true;
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SpawnBuildingRequest, Entity);
        }
    }

    // Face the build area center (NOT WorkerStats.BuildAreaPosition, which is the worker's own
    // stand-point on the area edge -> LookDir was ~0, so facing never really worked).
    FaceBuildArea(Context, EntityIdx);
}

void UBuildStateProcessor::ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, 
    FMassAIStateFragment& AIState, FMassWorkerStatsFragment& WorkerStats, const FMassEntityHandle Entity, const int32 EntityIdx)
{
    // 1. Lokales Hochzählen des Timers (da dieser nicht repliziert wird)
    AIState.StateTimer += ExecutionInterval;
    AIState.DeltaTime = ExecutionInterval;

    // Face the build area locally on the client too. BuildArea is replicated, so the client can aim at
    // the same center the server does (rotation/yaw replication alone was not reliably orienting it).
    FaceBuildArea(Context, EntityIdx);

    if (SignalSubsystem)
    {
        // SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::SyncConstructionScale, Entity);
    }
}

void UBuildStateProcessor::FaceBuildArea(FMassExecutionContext& Context, const int32 EntityIdx)
{
    const TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
    if (!ActorList.IsValidIndex(EntityIdx))
    {
        return;
    }

    const AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(ActorList[EntityIdx].Get());
    if (!Worker || !IsValid(Worker->BuildArea))
    {
        return;
    }

    const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
    FTransform& CurrentTransform = TransformList[EntityIdx].GetMutableTransform();

    FVector LookDir = Worker->BuildArea->GetActorLocation() - CurrentTransform.GetLocation();
    LookDir.Z = 0.f;
    if (LookDir.IsNearlyZero())
    {
        return;
    }

    const FQuat TargetRotation = FRotationMatrix::MakeFromX(LookDir).ToQuat();
    const float AngleErrorDeg = FMath::RadiansToDegrees(CurrentTransform.GetRotation().AngularDistance(TargetRotation));
    if (AngleErrorDeg > BuildFacingToleranceDeg)
    {
        CurrentTransform.SetRotation(TargetRotation);
    }
}

void UBuildStateProcessor::CalculateConstructionScale(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    TArray<FMassEntityHandle> EntitiesCopy = Entities;

    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]()
    {
        UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
        if (!EntitySubsystem) return;

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
            if (!ActorFrag) continue;

            AUnitBase* UnitBase = Cast<AUnitBase>(ActorFrag->GetMutable());
            if (!UnitBase || !UnitBase->BuildArea || !UnitBase->BuildArea->ConstructionUnit) continue;

            AUnitBase* CU = UnitBase->BuildArea->ConstructionUnit;
            AWorkArea* WA = UnitBase->BuildArea;

            if (WA->Mesh)
            {
                // Gleiche Logik wie auf dem Server zur Berechnung der Passform
                FBox AreaBox = WA->Mesh->CalcBounds(WA->Mesh->GetRelativeTransform()).GetBox();
                FVector AreaSize = AreaBox.GetSize();
                
                // Wir nutzen die Bounds der CU, um die initiale Größe zu bestimmen
                FBox UnitBox = CU->GetComponentsBoundingBox(true);
                FVector CurrentScale = CU->GetActorScale3D();
                FVector UnitSize = UnitBox.GetSize();

                // Herausrechnen der aktuellen Skalierung, um die unskalierte Basis-Größe zu erhalten
                if (!CurrentScale.IsNearlyZero())
                {
                    UnitSize.X /= CurrentScale.X;
                    UnitSize.Y /= CurrentScale.Y;
                    UnitSize.Z /= CurrentScale.Z;
                }
                
                if (!UnitSize.IsNearlyZero(1e-3f) && AreaSize.X > KINDA_SMALL_NUMBER)
                {
                    const float Margin = 0.98f;
                    const float ScaleX = (AreaSize.X * Margin) / UnitSize.X;
                    const float ScaleY = (AreaSize.Y * Margin) / UnitSize.Y;
                    const float ScaleZ = (AreaSize.Z * Margin) / UnitSize.Z;
                    const float Uniform = FMath::Max(FMath::Min(ScaleX, ScaleY), 0.1f);
                    
                    FVector NewScale = FVector(Uniform, Uniform, Uniform);
                    
                    // Falls die CU spezifische Z-Skalierung erlaubt
                    if (AConstructionUnit* SpecCU = Cast<AConstructionUnit>(CU))
                    {
                        if (SpecCU->ScaleZ) NewScale.Z = ScaleZ;
                    }
                    
                    FVector FinalActorScale = NewScale * 2.f * WA->ScaleConstructionUnit;
                    CU->SetActorScale3D(FinalActorScale);

                    // Update Mass Fragment
                    if (UWorld* World = CU->GetWorld())
                    {
                        if (UMassActorSubsystem* ActorSubsystem = World->GetSubsystem<UMassActorSubsystem>())
                        {
                            FMassEntityHandle CUEntity = ActorSubsystem->GetEntityHandleFromActor(CU);
                            if (CUEntity.IsValid())
                            {
                                if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(CUEntity))
                                {
                                    CharFrag->Scale = FinalActorScale;
                                    CharFrag->PositionedTransform.SetScale3D(FinalActorScale);
                                    CharFrag->bTransformDirty = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    });
}
