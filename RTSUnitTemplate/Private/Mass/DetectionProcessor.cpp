// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/DetectionProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassEntityUtils.h" // Für Schleifen über alle Entitäten (ineffizient!)
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "MassSignalTypes.h" 
#include "MassStateTreeFragments.h"  // For FMassStateDeadTag
#include "MassNavigationFragments.h" // For FMassAgentCharacteristicsFragment
#include "Async/Async.h"
#include "Mass/Signals/UnitSignalingProcessor.h"


UDetectionProcessor::UDetectionProcessor(): EntityQuery()
{
    // Sollte vor der State Machine laufen
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UDetectionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    World = Owner.GetWorld();
    if (World)
    {
        SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
        EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        
        if (SignalSubsystem)
        {

            if (SignalDelegateHandle.IsValid())
            {
                SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitInDetectionRange).Remove(SignalDelegateHandle);
            }

            SignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitInDetectionRange)
                                      .AddUObject(this, &UDetectionProcessor::HandleUnitPresenceSignal);

        }
    }
}

void UDetectionProcessor::BeginDestroy()
{
    if (SignalSubsystem && SignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
    {
        SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitInDetectionRange)
        .Remove(SignalDelegateHandle);
            
        SignalDelegateHandle.Reset();
    }

    Super::BeginDestroy();
}

// Called by the Signal Subsystem when the corresponding signal is processed
void UDetectionProcessor::HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{

  ReceivedSignalsBuffer.FindOrAdd(SignalName).Append(Entities.GetData(), Entities.Num());

}

void UDetectionProcessor::InjectCurrentTargetIfMissing(const FDetectorUnitInfo& DetectorInfo,
    TArray<FTargetUnitInfo>& InOutTargetUnits, FMassEntityManager& EntityManager)
{
    // 1. Check if the detector has a valid target stored.
    if (DetectorInfo.TargetFrag->bHasValidTarget && DetectorInfo.TargetFrag->TargetEntity.IsSet())
    {
        const FMassEntityHandle CurrentTargetEntity = DetectorInfo.TargetFrag->TargetEntity;

        // 2. Check if this target is already in our list of potential targets.
        const bool bAlreadyInList = InOutTargetUnits.ContainsByPredicate(
            [&CurrentTargetEntity](const FTargetUnitInfo& Info) {
                return Info.Entity == CurrentTargetEntity;
            });

        // 3. If it's NOT in the list, we need to add it.
        if (!bAlreadyInList)
        {
            // We must fetch its data directly from the EntityManager to build the FTargetUnitInfo struct.
            if (EntityManager.IsEntityValid(CurrentTargetEntity))
            {
                FTransformFragment* TgtTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(CurrentTargetEntity);
                FMassAIStateFragment* TgtState = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(CurrentTargetEntity);
                const FMassCombatStatsFragment* TgtStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(CurrentTargetEntity);
                const FMassAgentCharacteristicsFragment* TgtChar = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(CurrentTargetEntity);

                // Ensure all data is valid before adding
                if (TgtTransformFrag && TgtState && TgtStats && TgtChar)
                {
                    if (TgtStats->Health >= 0)
                    {
                        InOutTargetUnits.Add({
                            CurrentTargetEntity,
                            TgtTransformFrag->GetTransform().GetLocation(),
                            TgtState,
                            TgtStats,
                            TgtChar
                            });
                    }
                }
            }
        }
    }
}

void UDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    // Tell the base class which signal we care about:
    EntityQuery.Initialize(EntityManager);
    // Dieser Prozessor läuft für alle Einheiten, die Ziele erfassen sollen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel schreiben
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Fähigkeiten lesen
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddTagRequirement<FMassStateDetectTag>(EMassFragmentPresence::All);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

    
    EntityQuery.RegisterWithProcessor(*this);
}


void UDetectionProcessor::Execute(
    FMassEntityManager&   EntityManager,
    FMassExecutionContext& Context)
{
    // 1) Timer
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun = 0.f;
    
    TArray<FMassEntityHandle> PotentialTargets;
    if (const TArray<FMassEntityHandle>* ReceivedEntities = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange))
    {
        PotentialTargets = *ReceivedEntities;
    }
    
    // NEW: Clear the buffer for the next frame.
    ReceivedSignalsBuffer.Reset();

    TArray<FTargetUnitInfo> TargetUnits;
    TargetUnits.Reserve(256);
    
    for (const FMassEntityHandle& TgtEntity : PotentialTargets)
    {
        if (!EntityManager.IsEntityValid(TgtEntity)) continue;

        // Fetch all required fragments for this target
        FTransformFragment* TgtTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TgtEntity);
        FMassAIStateFragment* TgtState = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(TgtEntity);
        const FMassCombatStatsFragment* TgtStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TgtEntity);
        const FMassAgentCharacteristicsFragment* TgtChar = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TgtEntity);
        const FMassSightFragment* SightFragment = EntityManager.GetFragmentDataPtr<FMassSightFragment>(TgtEntity);
        const float Age = World->GetTimeSeconds() - TgtState->BirthTime;
        if (Age < 1.f) 
            continue;
        
        TargetUnits.Add({
                TgtEntity,
                TgtTransformFrag->GetTransform().GetLocation(),
                TgtState,
                //AiTgtFrag,
                TgtStats,
                TgtChar,
                SightFragment
            });
    }
    
    TArray<FDetectorUnitInfo> DetectorUnits;
    DetectorUnits.Reserve(256);
    
    
    EntityQuery.ForEachEntityChunk(Context,
        [&DetectorUnits, this](FMassExecutionContext& ChunkCtx)
    {
        const int32 N = ChunkCtx.GetNumEntities();
        const auto& Transforms    = ChunkCtx.GetFragmentView<FTransformFragment>();
        auto*        StateList    = ChunkCtx.GetMutableFragmentView<FMassAIStateFragment>().GetData();
        auto*        TargetList   = ChunkCtx.GetMutableFragmentView<FMassAITargetFragment>().GetData();
        //auto*        MoveList     = ChunkCtx.GetMutableFragmentView<FMassMoveTargetFragment>().GetData();
        const auto&  StatsList    = ChunkCtx.GetFragmentView<FMassCombatStatsFragment>();
        const auto&  CharList     = ChunkCtx.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < N; ++i)
        {
            // skip too‐young
            const float Age = World->GetTimeSeconds() - StateList[i].BirthTime;
            if (Age < 1.f) 
                continue;

            DetectorUnits.Add({
                ChunkCtx.GetEntity(i),
                Transforms[i].GetTransform().GetLocation(),
                &StateList[i],
                &TargetList[i],
                //&MoveList[i],
                &StatsList[i],
                &CharList[i]
            });
        }
    });

    // 3) For each detector, scan every other unit to pick BestEntity / CurrentStillViable
    TArray<FMassSignalPayload> PendingSignals;
    PendingSignals.Reserve(DetectorUnits.Num());

    for (auto& Det : DetectorUnits)
    {
        const float Now = World->GetTimeSeconds();

        FMassEntityHandle BestEntity;
        FVector BestLocation = FVector::ZeroVector;
        bool bFoundNew = false;
        bool bCurrentStillViable = false;
        FVector CurrentLocation = FVector::ZeroVector;
        float DetectorSight = FMath::Square(Det.Stats->SightRadius);
        const int32 DetectorTeamId = Det.Stats->TeamId;
        InjectCurrentTargetIfMissing(Det, TargetUnits, EntityManager);
        // Add  Det.TargetFrag->TargetEntity to TargetUnits if it is not allready inside
        if (!Det.TargetFrag->IsFocusedOnTarget)
        {
            for (auto& Tgt : TargetUnits)
            {
                if (Tgt.Entity == Det.Entity) 
                    continue;
                
                // skip friendly
                if (Tgt.Stats->TeamId == Det.Stats->TeamId)
                    continue;
                

                // skip too‐young / too‐old
                const float TgtAge = Now - Tgt.State->BirthTime;
                const float SinceDeath = Now - Tgt.State->DeathTime;
                if (TgtAge < 1.f || SinceDeath > 4.f) 
                    continue;

                // can I attack their type?
                if ((Det.Char->bCanOnlyAttackGround && Tgt.Char->bIsFlying) ||
                    (Det.Char->bCanOnlyAttackFlying && !Tgt.Char->bIsFlying) ||
                    (Tgt.Char->bIsInvisible && !Det.Char->bCanDetectInvisible))
                {
                    continue;
                }

                
                float DistSq = FVector::DistSquared2D(Det.Location, Tgt.Location);
                
                // “new” target if in sight radius and closer than anything before, and alive
                if (DistSq < DetectorSight && Tgt.Stats->Health > 0)
                {
                    BestEntity     = Tgt.Entity;
                    BestLocation   = Tgt.Location;
                    bFoundNew      = true;
                    DetectorSight = DistSq;
                }

                // “current” target still viable if it’s the same one and within lose‐sight radius
                if (Tgt.Entity == Det.TargetFrag->TargetEntity && DistSq < FMath::Square(Det.Stats->LoseSightRadius) && Tgt.Stats->Health > 0)
                {
                    CurrentLocation    = Tgt.Location;
                    bCurrentStillViable = true;
                }


                if (!bFoundNew && !bCurrentStillViable)
                {
   
                    const int32* AttackingSightCount = Tgt.Sight->ConsistentAttackerTeamOverlapsPerTeam.Find(DetectorTeamId);
                    
                    if (Tgt.Stats->Health > 0 && AttackingSightCount && *AttackingSightCount > 0 && DistSq < FMath::Square(Tgt.Stats->LoseSightRadius))
                    {
                        BestEntity     = Tgt.Entity;
                        BestLocation   = Tgt.Location;
                        bFoundNew      = true;
                    }
                    
                }
            }

            /*
            // "Call to Arms" logic: If no target was found in sight, check for allies needing help.
            if (!bFoundNew && !bCurrentStillViable)
            {
                // New variable to track the closest "assist" target. Initialize to a very large value.
                float BestAssistDistSq = TNumericLimits<float>::Max();

                for (auto& Tgt : TargetUnits)
                {
                    // Basic checks (don't assist against friendlies, etc.)
                    if (Tgt.Entity == Det.Entity || Tgt.Stats->TeamId == DetectorTeamId || Tgt.Stats->Health <= 0)
                        continue;
                    
                    const int32* AttackingSightCount = Tgt.Sight->ConsistentAttackerTeamOverlapsPerTeam.Find(DetectorTeamId);

                    // Check if an ally is attacking this target
                    if (AttackingSightCount && *AttackingSightCount > 0)
                    {
                        const float DistSq = FVector::DistSquared2D(Det.Location, Tgt.Location);

                        // Is this "assist" target closer than the previous best "assist" target?
                        if (DistSq < FMath::Square(Tgt.Stats->LoseSightRadius) && DistSq < BestAssistDistSq)
                        {
                            BestEntity       = Tgt.Entity;
                            BestLocation     = Tgt.Location;
                            bFoundNew        = true;
                            // IMPORTANT: Update the closest distance so we can compare against it.
                            BestAssistDistSq = DistSq;
                        }
                    }
                }
            }
            */
            /*
            if (!bFoundNew && !bCurrentStillViable)
            {
                for (auto& Tgt : TargetUnits)
                {
                    const int32* AttackingSightCount = Tgt.Sight->ConsistentAttackerTeamOverlapsPerTeam.Find(DetectorTeamId);

                    float DistSq = FVector::DistSquared2D(Det.Location, Tgt.Location);
                    if (Tgt.Stats->Health > 0 && AttackingSightCount && *AttackingSightCount > 0 && DistSq < FMath::Square(Tgt.Stats->LoseSightRadius))
                    {
                        BestEntity     = Tgt.Entity;
                        BestLocation   = Tgt.Location;
                        bFoundNew      = true;
                    }
                }
            }*/
        }
        else
        {
            for (auto& Tgt : TargetUnits)
            {
                if (Tgt.Entity != Det.TargetFrag->TargetEntity) 
                    continue;
                
               // const int32 DetectorTeamId = Det.Stats->TeamId;
                
                const int32* SightCount = Tgt.Sight->ConsistentTeamOverlapsPerTeam.Find(DetectorTeamId);
                const int32* DetectorSightCount = Tgt.Sight->ConsistentDetectorOverlapsPerTeam.Find(DetectorTeamId);
      
                if (Tgt.Stats->TeamId == Det.Stats->TeamId && Tgt.Entity == Det.TargetFrag->TargetEntity && Tgt.Stats->Health > 0)
                {
                    CurrentLocation    = Tgt.Location;
                    bCurrentStillViable = true;
                }else if (Tgt.Entity == Det.TargetFrag->TargetEntity &&
                    Tgt.Stats->Health > 0 &&
                    ((!Tgt.Char->bIsInvisible && SightCount && *SightCount > 0) || (Tgt.Char->bIsInvisible && DetectorSightCount && *DetectorSightCount > 0)))
                {
                    CurrentLocation    = Tgt.Location;
                    bCurrentStillViable = true;
                }else
                {
                    Det.TargetFrag->IsFocusedOnTarget = false;
                }
            }
        }

        // 4) Update target fragment
        if (bFoundNew && !Det.TargetFrag->IsFocusedOnTarget)
        {
            Det.TargetFrag->TargetEntity      = BestEntity;
            Det.TargetFrag->LastKnownLocation = BestLocation;
            Det.TargetFrag->bHasValidTarget   = true;
            if (!Det.State->SwitchingState)
            {
                PendingSignals.Emplace(Det.Entity, UnitSignals::SetUnitToChase);
            }
        }
        else if (bCurrentStillViable)
        {
            Det.TargetFrag->LastKnownLocation = CurrentLocation;
            Det.TargetFrag->bHasValidTarget   = true;
        }
        else
        {
            Det.TargetFrag->TargetEntity.Reset();
            Det.TargetFrag->bHasValidTarget = false;
        }
    }
    
    if (!PendingSignals.IsEmpty() && SignalSubsystem)
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
                        Sub->SignalEntity(P.SignalName, P.TargetEntity);
                    }
                }
            }
        });
    }
}