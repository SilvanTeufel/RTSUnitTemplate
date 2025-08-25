// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/UnitSightProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/DetectionProcessor.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

UUnitSightProcessor::UUnitSightProcessor(): EntityQuery()
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
    bRequiresGameThreadExecution = true;
}

void UUnitSightProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    World = Owner.GetWorld();
    if (World)
    {
        SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
        if (!SignalSubsystem) return;
        
        EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    }
  
}

void UUnitSightProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    // Query all entities with transform, combat stats, and our AI state fragment
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSightProcessor::Execute(
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
        FMassAgentCharacteristicsFragment*          Char;    // <--- mutable pointer
        FMassAIStateFragment*                       State;
        FMassSightFragment*                         Sight;
    };
    TArray<FLocalInfo> AllEntities;
    AllEntities.Reserve(256);

    EntityQuery.ForEachEntityChunk(Context,
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
            FMassAIStateFragment& State = StateList[i];
            // skip super-young or long-dead
            const float Age   = World->GetTimeSeconds() - State.BirthTime;
            const float SinceDeath = World->GetTimeSeconds() - State.DeathTime;
            if (Age < 1.f || SinceDeath > 4.f || !State.IsInitialized)
            {
                continue;
            }

            AllEntities.Add({
                ChunkCtx.GetEntity(i),
                Transforms[i].GetTransform().GetLocation(),
                &StatsList[i],
                &CharList[i],
                &StateList[i],
                &SightList[i]
            });
        }
    });

    // 3) Now do your O(M²) pass exactly once over that flat list
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

            // filter by team
            if (Det.Stats->TeamId == Tgt.Stats->TeamId) 
                continue;

            const float DistSqr = FVector::DistSquared(Det.Location, Tgt.Location);
            if (DistSqr > FMath::Square(Det.Stats->SightRadius)) 
                continue;

            // accumulate per-detector & per-team overlap counts
            if (Det.Char->bCanDetectInvisible || !Tgt.Char->bCanBeInvisible)
            {
                Tgt.Sight->DetectorOverlapsPerTeam.FindOrAdd(Det.Stats->TeamId)++;
            }

            //UE_LOG(LogTemp, Log, TEXT("THIS IS IN SIGHT!"));
            Tgt.Sight->TeamOverlapsPerTeam.FindOrAdd(Det.Stats->TeamId)++;
        }
    }

    // 4) Emit signals & update invisibility
    for (auto& Detector : AllEntities)
    {
        const int32 DetectorTeamId = Detector.Stats->TeamId;
        
        for (auto& Target : AllEntities)
        {
            if (Target.Stats->TeamId == Detector.Stats->TeamId) continue;
            
            const int32 SightCount = Target.Sight->TeamOverlapsPerTeam.FindOrAdd(DetectorTeamId);
            //UE_LOG(LogTemp, Log, TEXT("SightCount: %d"), SightCount);
            if (SightCount > 0)
            {
                PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitEnterSight);
            }
            else
            {
                PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitExitSight);
            }

       
            const int32 DetectCount = Target.Sight->DetectorOverlapsPerTeam.FindOrAdd(DetectorTeamId);
            if (DetectCount > 0)
            {
                Target.Char->bIsInvisible = false;
            }
            else if (Target.Char->bCanBeInvisible)
            {
                Target.Char->bIsInvisible = true;
            }
            // clear for next tick
        }
    }


    for (auto& Target : AllEntities)
    {
        Target.Sight->TeamOverlapsPerTeam.Empty();
        Target.Sight->DetectorOverlapsPerTeam.Empty();
    }
    // 5) Update fog mask once
    if (SignalSubsystem && FogEntities.Num() > 0)
    {
        SignalSubsystem->SignalEntities(UnitSignals::UpdateFogMask, FogEntities);
    }

    // 6) Dispatch sight signals once
    if (SignalSubsystem && PendingSignals.Num() > 0)
    {
        TWeakObjectPtr<UMassSignalSubsystem> SubPtr = SignalSubsystem;
        AsyncTask(ENamedThreads::GameThread,
            [SubPtr, Signals = MoveTemp(PendingSignals)]()
        {
            if (auto* Sub = SubPtr.Get())
            {
                for (auto& P : Signals)
                {
                    if (!P.SignalName.IsNone())
                    {
                        Sub->SignalEntities(P.SignalName,
                            { P.TargetEntity, P.DetectorEntity });
                    }
                }
            }
        });
    }
}