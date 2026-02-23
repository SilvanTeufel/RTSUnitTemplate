// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitSightProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/DetectionProcessor.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "MassActorSubsystem.h" // FMassActorFragment
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Controller/PlayerController/CustomControllerBase.h"

UUnitSightProcessor::UUnitSightProcessor(): EntityQuery()
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Client);
    bRequiresGameThreadExecution = true;
}

void UUnitSightProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    World = Owner.GetWorld();

    if (!World)
    {
        return;
    }

    SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();

    if (!SignalSubsystem)
    {
        return;
    }

    // Bind sight-related signals directly to this processor (runs on client and server)
    {
        FDelegateHandle H1 = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitEnterSight)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitSightProcessor, HandleSightSignals));
        SightSignalDelegateHandles.Add(H1);

        FDelegateHandle H2 = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitExitSight)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitSightProcessor, HandleSightSignals));
        SightSignalDelegateHandles.Add(H2);
    }

    // Bind fog update so clients can update Fog + Minimap locally
    FogParametersDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateFogMask)
        .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitSightProcessor, HandleUpdateFogMask));
}

void UUnitSightProcessor::HandleUpdateFogMask(FName /*SignalName*/, TArray<FMassEntityHandle>& Entities)
{
    if (!World)
    {
        return;
    }
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }
    ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
    if (!CustomPC)
    {
        return;
    }
    // Copy entity array for async safety
    TArray<FMassEntityHandle> Copied = Entities;
    AsyncTask(ENamedThreads::GameThread, [CustomPC, Copied = MoveTemp(Copied)]()
    {
        CustomPC->UpdateFogMaskWithCircles(Copied);
        CustomPC->UpdateMinimap(Copied);
    });
}

void UUnitSightProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    // Server/main query with full set of requirements
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.RegisterWithProcessor(*this);

    // Query for EffectAreas
    EffectAreaQuery.Initialize(EntityManager);
    EffectAreaQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EffectAreaQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EffectAreaQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadWrite);
    EffectAreaQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EffectAreaQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
    EffectAreaQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::All);
    EffectAreaQuery.RegisterWithProcessor(*this);
}

void UUnitSightProcessor::Execute(
    FMassEntityManager&   EntityManager,
    FMassExecutionContext& Context)
{
        ExecuteServer(EntityManager, Context);
}

void UUnitSightProcessor::ExecuteServer(
    FMassEntityManager&   EntityManager,
    FMassExecutionContext& Context)
{
    // 1) Accumulate time
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun = 0.f;
    
    // 2) Gather every “alive” entity into a flat array
    struct FLocalInfo
    {
        FMassEntityHandle                           Entity;
        FVector                                     Location;
        const FMassCombatStatsFragment*             Stats;   
        FMassAgentCharacteristicsFragment*          Char;
        FMassAIStateFragment*                       State;
        FMassSightFragment*                         Sight;
    };
    TArray<FLocalInfo> AllEntities;
    AllEntities.Reserve(256);
    
    auto GatherEntities = [&](FMassEntityQuery& Query)
    {
        Query.ForEachEntityChunk(Context,
            [&AllEntities, this](FMassExecutionContext& ChunkCtx)
        {
            const int32 N = ChunkCtx.GetNumEntities();
            const auto& Transforms = ChunkCtx.GetFragmentView<FTransformFragment>();
            const auto& StatsList  = ChunkCtx.GetFragmentView<FMassCombatStatsFragment>();
            auto*        StateList = ChunkCtx.GetMutableFragmentView<FMassAIStateFragment>().GetData();
            auto*        SightList = ChunkCtx.GetMutableFragmentView<FMassSightFragment>().GetData();
            auto*        CharList  = ChunkCtx.GetMutableFragmentView<FMassAgentCharacteristicsFragment>().GetData();
        
            for (int32 i = 0; i < N; ++i)
            {
                if (StateList)
                {
                    FMassAIStateFragment& State = StateList[i];
                    const float Age = World->GetTimeSeconds() - State.BirthTime;
                    const float SinceDeath = World->GetTimeSeconds() - State.DeathTime;

                    const bool bIsTooYoung = (Age < 1.f && Age >= 0.f);
                    const bool bIsDeadTooLong = (SinceDeath > 4.f && SinceDeath >= 0.f);

                    if (bIsTooYoung || bIsDeadTooLong || !State.IsInitialized)
                    {
                        continue;
                    }
                }

                AllEntities.Add({
                    ChunkCtx.GetEntity(i),
                    Transforms[i].GetTransform().GetLocation(),
                    &StatsList[i],
                    &CharList[i],
                    StateList ? &StateList[i] : nullptr,
                    &SightList[i]
                });
            }
        });
    };

    GatherEntities(EntityQuery);
    GatherEntities(EffectAreaQuery);

    // 3) Calculate Overlaps
    TArray<FMassEntityHandle>         FogEntities;
    TArray<FMassSightSignalPayload>   PendingSignals;
    FogEntities.Reserve(AllEntities.Num());
    PendingSignals.Reserve(AllEntities.Num() * 2);
    
    for (int32 i = 0; i < AllEntities.Num(); ++i)
    {
        const auto& Det = AllEntities[i];
        FogEntities.Add(Det.Entity);

        for (int32 j = 0; j < AllEntities.Num(); ++j)
        {
            if (i == j) continue;
            const auto& Tgt = AllEntities[j];

            if (Det.Stats->TeamId == Tgt.Stats->TeamId) 
                continue;
            
            const float DistSqr = FVector::DistSquared2D(Det.Location, Tgt.Location);
            const float SightR2 = FMath::Square(Det.Stats->SightRadius);

            if (DistSqr > SightR2) 
                continue;
            
            if (Det.Char->bCanDetectInvisible || !Tgt.Char->bCanBeInvisible)
            {
                Tgt.Sight->DetectorOverlapsPerTeam.FindOrAdd(Det.Stats->TeamId)++;
            }
            
            Tgt.Sight->TeamOverlapsPerTeam.FindOrAdd(Det.Stats->TeamId)++;
        }
    }

    // 4) Update global invisibility status
    for (auto& Target : AllEntities)
    {
        if (Target.Char->bCanBeInvisible)
        {
            bool bAnyEnemyDetects = false;
            for (auto& Pair : Target.Sight->DetectorOverlapsPerTeam)
            {
                if (Pair.Value > 0)
                {
                    bAnyEnemyDetects = true;
                    break;
                }
            }
            Target.Char->bIsInvisible = !bAnyEnemyDetects;
        }
        else
        {
            Target.Char->bIsInvisible = false;
        }
    }

    // 5) Enqueue Signals
    for (auto& Detector : AllEntities)
    {
        const int32 DetectorTeamId = Detector.Stats->TeamId;
        
        for (auto& Target : AllEntities)
        {
            if (Target.Stats->TeamId == DetectorTeamId) continue;
            
            const int32 DetectCount = Target.Sight->DetectorOverlapsPerTeam.FindOrAdd(DetectorTeamId);
            
            if (DetectCount > 0)
            {
                PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitEnterSight);
            }
            else
            {
                int32 AttackingSightCount = Target.Sight->AttackerTeamOverlapsPerTeam.FindOrAdd(DetectorTeamId);
                
                if (AttackingSightCount > 0)
                {
                    const float DistSqr = FVector::DistSquared2D(Detector.Location, Target.Location);
                    if (DistSqr <= FMath::Square(Target.Stats->SightRadius))
                    {
                        PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitEnterSight);
                    }
                    else
                    {
                        PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitExitSight);
                    }
                }
                else
                {
                    PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitExitSight);
                }
            }
        }
    }

    // 6) Sync Consistent Overlaps and Reset
    for (auto& Target : AllEntities)
    {
        Target.Sight->ConsistentDetectorOverlapsPerTeam = Target.Sight->DetectorOverlapsPerTeam;
        Target.Sight->ConsistentTeamOverlapsPerTeam = Target.Sight->TeamOverlapsPerTeam;
        Target.Sight->ConsistentAttackerTeamOverlapsPerTeam = Target.Sight->AttackerTeamOverlapsPerTeam;
        Target.Sight->TeamOverlapsPerTeam.Empty();
        Target.Sight->DetectorOverlapsPerTeam.Empty();
    }

    // 7) Update fog mask and dispatch signals
    if (SignalSubsystem)
    {
        if (FogEntities.Num() > 0)
        {
            SignalSubsystem->SignalEntities(UnitSignals::UpdateFogMask, FogEntities);
        }

        if (PendingSignals.Num() > 0)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SubPtr = SignalSubsystem;
            AsyncTask(ENamedThreads::GameThread, [SubPtr, Signals = MoveTemp(PendingSignals)]()
            {
                if (auto* Sub = SubPtr.Get())
                {
                    for (auto& P : Signals)
                    {
                        if (!P.SignalName.IsNone())
                        {
                            Sub->SignalEntities(P.SignalName, { P.TargetEntity, P.DetectorEntity });
                        }
                    }
                }
            });
        }
    }
}

void UUnitSightProcessor::HandleSightSignals(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem)
    {
        return;
    }

    if (Entities.Num() < 2)
    {
        return;
    }

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

    const bool bValid0 = EntityManager.IsEntityValid(Entities[0]);
    const bool bValid1 = EntityManager.IsEntityValid(Entities[1]);
    if (!bValid0 || !bValid1)
    {
        return;
    }

    FMassActorFragment* TargetActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entities[0]);
    FMassActorFragment* DetectorActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entities[1]);

    if (!TargetActorFragPtr || !DetectorActorFragPtr)
    {
        return;
    }

    AActor* TargetActor = TargetActorFragPtr->GetMutable();
    AActor* DetectorActor = DetectorActorFragPtr->GetMutable();

    const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;

    if (IsValid(TargetActor))
    {
        IMassVisibilityInterface* VisInterface = Cast<IMassVisibilityInterface>(TargetActor);
        if (VisInterface)
        {
            if (SignalName == UnitSignals::UnitEnterSight)
            {
                VisInterface->SetEnemyVisibility(DetectorActor, true);
            }
            else if (SignalName == UnitSignals::UnitExitSight)
            {
                VisInterface->SetEnemyVisibility(DetectorActor, false);
            }
        }
    }
}