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
  
            // On clients, BirthTime is replicated from the server's clock.
            // If the server has been running longer than the client, Age will be negative.
            // We only want to skip the 1s "warmup" if the unit was JUST born on the current instance.
            const bool bIsTooYoung = (Age < 1.f && Age >= 0.f);
            const bool bIsDeadTooLong = (SinceDeath > 4.f && SinceDeath >= 0.f);
            
            if (bIsTooYoung || bIsDeadTooLong || !State.IsInitialized)
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
    
    int32 PairLogCount = 0;
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


    int Rounds = 0;
    
    for (auto& Detector : AllEntities)
    {
        const int32 DetectorTeamId = Detector.Stats->TeamId;
        
        for (auto& Target : AllEntities)
        {
            if (Target.Stats->TeamId == Detector.Stats->TeamId) continue;
            
            const int32 SightCount = Target.Sight->TeamOverlapsPerTeam.FindOrAdd(DetectorTeamId);
            
            if (SightCount > 0)
            {
                PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitEnterSight);
            }
            else
            {

                int32 AttackingSightCount = Target.Sight->AttackerTeamOverlapsPerTeam.FindOrAdd(DetectorTeamId);
                
                if (AttackingSightCount > 0 )
                {
                    // 2. If so, check if this specific detector is within the TARGET's lose-sight radius.
                    const float DistSqr = FVector::DistSquared2D(Detector.Location, Target.Location);
                    if (DistSqr <= FMath::Square(Target.Stats->SightRadius))
                    {
                        // The target is revealed to this detector due to nearby combat.
                        PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitEnterSight);
                    }
                    else
                    {
                        // The detector is too far away to see the revealed target.
                        PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitExitSight);
                    }
                }else
                {
                    // Not in direct sight and not revealed by combat, so this detector can't see the target.
                    PendingSignals.Emplace(Target.Entity, Detector.Entity, UnitSignals::UnitExitSight);
                }
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
        }

        Rounds++;
    }


    for (auto& Target : AllEntities)
    {
        Target.Sight->DetectorOverlapsPerTeam = Target.Sight->DetectorOverlapsPerTeam;
        Target.Sight->ConsistentTeamOverlapsPerTeam = Target.Sight->TeamOverlapsPerTeam;
        Target.Sight->ConsistentAttackerTeamOverlapsPerTeam = Target.Sight->AttackerTeamOverlapsPerTeam;
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
        APerformanceUnit* TargetUnit = Cast<APerformanceUnit>(TargetActor);
        APerformanceUnit* DetectorUnit = Cast<APerformanceUnit>(DetectorActor);
        if (TargetUnit)
        {
            if (SignalName == UnitSignals::UnitEnterSight)
            {
                TargetUnit->SetEnemyVisibility(DetectorUnit, true);
            }
            else if (SignalName == UnitSignals::UnitExitSight)
            {
                TargetUnit->SetEnemyVisibility(DetectorUnit, false);
            }
        }
    }
}