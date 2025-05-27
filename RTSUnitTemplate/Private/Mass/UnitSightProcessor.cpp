#include "Mass/UnitSightProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/DetectionProcessor.h"
#include "Mass/Signals/MySignals.h"

UUnitSightProcessor::UUnitSightProcessor()
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UUnitSightProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    World = Owner.GetWorld();
    if (World)
    {
        SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
        EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        
        if (SignalSubsystem)
        {
            // Ensure previous handle is removed if Initialize is called multiple times (unlikely for processors, but safe)
            
            if (SignalDelegateHandle.IsValid())
            {
                SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitInDetectionRange).Remove(SignalDelegateHandle);
            }
            // Bind our handler function
            SignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitInDetectionRange)
                                      .AddUObject(this, &UUnitSightProcessor::HandleUnitPresenceSignal);
            //UE_LOG(LogMass, Log, TEXT("UDetectionProcessor bound to signal '%s'"), *UnitSignals::UnitInDetectionRange.ToString());
        }
    }
}

void UUnitSightProcessor::ConfigureQueries()
{
    // Query all entities with transform, combat stats, and our AI state fragment
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSightProcessor::HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
    // Append the received entities to our buffer for processing in Execute
    // This function might be called multiple times per frame if signals are dispatched in batches
    ReceivedSignalsBuffer.FindOrAdd(SignalName).Append(Entities.GetData(), Entities.Num());
    // UE_LOG(LogMass, Verbose, TEXT("HandleUnitPresenceSignal: Received %d entities for signal %s"), Entities.Num(), *SignalName.ToString());
}

void UUnitSightProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun = 0.f;

    // 3. Pull in the raw detection‐range signals this tick
    TArray<FMassEntityHandle>* SignaledEntitiesPtr = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange);
    const TArray<FMassEntityHandle> SignaledEntities = SignaledEntitiesPtr ? *SignaledEntitiesPtr : TArray<FMassEntityHandle>{};

    // --- Pre-fetch fragment data for signaled targets ---
    struct FCachedSightData
    {
        FMassEntityHandle Entity;
        FTransform             Transform;
        FMassCombatStatsFragment CombatStats;
        FMassAIStateFragment     State;
        FMassAgentCharacteristicsFragment Char;
        bool                   bValid = false;
    };

    TArray<FCachedSightData> CachedTargets;
    CachedTargets.Reserve(SignaledEntities.Num());

    for (const FMassEntityHandle& Handle : SignaledEntities)
    {
        FCachedSightData Data;
        Data.Entity = Handle;
        
        if (!EntityManager.IsEntityValid(Handle))
        {
            continue;
        }
        
        // Check required fragments
        if (DoesEntityHaveFragment<FTransformFragment>(EntityManager, Handle) &&
            DoesEntityHaveFragment<FMassCombatStatsFragment>(EntityManager, Handle) &&
            DoesEntityHaveFragment<FMassAIStateFragment>(EntityManager, Handle) &&
            DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EntityManager, Handle))
        {
            // Cache fragments
            const FTransformFragment* Tf    = EntityManager.GetFragmentDataPtr<FTransformFragment>(Handle);
            const FMassCombatStatsFragment* Stats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Handle);
            const FMassAIStateFragment*    St   = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Handle);
            const FMassAgentCharacteristicsFragment* Ch = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Handle);

            Data.Transform       = Tf->GetTransform();
            Data.CombatStats     = *Stats;
            Data.State           = *St;
            Data.Char            = *Ch;
            Data.bValid          = true;
        }
        
        CachedTargets.Add(Data);
    }

    // 4. Reset per-frame state on detectors
    TArray<FMassEntityHandle> FogEntities;
    TArray<FMassSightSignalPayload> PendingSignals;

    for (const FMassEntityHandle& T : SignaledEntities)
    {
        if (EntityManager.IsEntityValid(T))
        {
            auto& State = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(T);
            State.TeamOverlapsPerTeam.Empty();
            State.DetectorOverlapsPerTeam.Empty();
        }
    }

    // 5. Chunk loop: detect who sees whom
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, &PendingSignals, &EntityManager, &FogEntities, &CachedTargets](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList    = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto& Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto& StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto& CharList  = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const float Now = World->GetTimeSeconds();
        
        // First pass: accumulate overlaps
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& DetectorState = StateList[i];
            if (Now - DetectorState.BirthTime < 1.0f)
            {
                continue;
            }

            FMassEntityHandle Detector = ChunkContext.GetEntity(i);
            const FVector DetectLoc = Transforms[i].GetTransform().GetLocation();
            const FMassCombatStatsFragment& DetStats = StatsList[i];
            const FMassAgentCharacteristicsFragment& DetChar = CharList[i];

            // collect for fog-of-war
            FogEntities.Add(Detector);

            for (const FCachedSightData& Target : CachedTargets)
            {
                if (!Target.bValid || Target.Entity == Detector)
                {
                    continue;
                }
                // Skip same team
                if (DetStats.TeamId == Target.CombatStats.TeamId)
                {
                    continue;
                }
                // Distance check
                float Dist = FVector::Dist(DetectLoc, Target.Transform.GetLocation());
                if (Dist <= DetStats.SightRadius)
                {
                    // visibility check
                    if (DetChar.bCanDetectInvisible || !Target.Char.bCanBeInvisible)
                    {
                        int32& Count = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(Target.Entity)
                            .DetectorOverlapsPerTeam.FindOrAdd(DetStats.TeamId);
                        Count++;
                    }
                    int32& TeamCount = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(Target.Entity)
                        .TeamOverlapsPerTeam.FindOrAdd(DetStats.TeamId);
                    TeamCount++;
                }
            }
        }

        // Second pass: emit signals and update invisibility
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassEntityHandle Detector = ChunkContext.GetEntity(i);
            const FMassCombatStatsFragment& DetStats = StatsList[i];

            for (const FCachedSightData& Target : CachedTargets)
            {
                if (!Target.bValid || Target.Entity == Detector)
                {
                    continue;
                }
                auto& TargetState = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(Target.Entity);
                auto& TargetChar  = EntityManager.GetFragmentDataChecked<FMassAgentCharacteristicsFragment>(Target.Entity);

                int32& SightCount = TargetState.TeamOverlapsPerTeam.FindOrAdd(DetStats.TeamId);
                if (SightCount > 0)
                {
                    PendingSignals.Emplace(Target.Entity, Detector, UnitSignals::UnitEnterSight);
                }
                else
                {
                    PendingSignals.Emplace(Target.Entity, Detector, UnitSignals::UnitExitSight);
                }

                int32& DetectCount = TargetState.DetectorOverlapsPerTeam.FindOrAdd(DetStats.TeamId);
                if (DetectCount > 0)
                {
                    TargetChar.bIsInvisible = false;
                }
                else if (TargetChar.bCanBeInvisible)
                {
                    TargetChar.bIsInvisible = true;
                }
            }
        }
    });

    // 6. Update fog
    if (SignalSubsystem && !FogEntities.IsEmpty())
    {
        SignalSubsystem->SignalEntities(UnitSignals::UpdateFogMask, FogEntities);
    }

    // 7. Dispatch sight signals
    if (!PendingSignals.IsEmpty() && SignalSubsystem)
    {
        TWeakObjectPtr<UMassSignalSubsystem> SubPtr = SignalSubsystem;
        AsyncTask(ENamedThreads::GameThread, [SubPtr, Signals = MoveTemp(PendingSignals)]()
        {
            if (UMassSignalSubsystem* Sub = SubPtr.Get())
            {
                for (const FMassSightSignalPayload& P : Signals)
                {
                    if (!P.SignalName.IsNone())
                    {
                        Sub->SignalEntities(P.SignalName, {P.TargetEntity, P.DetectorEntity});
                    }
                }
            }
        });
    }

    // 8. Cleanup signals
    if (SignaledEntitiesPtr)
    {
        ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    }
}
/*
void UUnitSightProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun = 0.f;

    // 3. Pull in the raw detection‐range signals this tick:
    TArray<FMassEntityHandle>* SignaledEntitiesPtr = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange);
    if (!SignaledEntitiesPtr || SignaledEntitiesPtr->IsEmpty())
    {
        // No presence signals received since last tick.
        // We MUST still run the target loss logic for existing targets.
        // Fall through to ForEachEntityChunk, but the inner loop over signaled entities won't run.
    }

    // Use a const reference for safety and potentially minor efficiency
    const TArray<FMassEntityHandle>& SignaledEntities = (SignaledEntitiesPtr) ? *SignaledEntitiesPtr : TArray<FMassEntityHandle>{}; // Empty array if ptr is null

    // 4. Prepare our per‐frame containers:

    TArray<FMassEntityHandle> FogEntities;
    TArray<FMassSightSignalPayload> PendingSignals;

    for (const FMassEntityHandle& T : SignaledEntities)
    {
        if (EntityManager.IsEntityValid(T))
        {
            auto& State = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(T);
            State.TeamOverlapsPerTeam.Empty();
            State.DetectorOverlapsPerTeam.Empty();
        }
    }

    // 5. Chunk loop: detect who sees whom this tick, collect FogEntities & build CurrentFrameSights
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
    [this, &PendingSignals, &EntityManager, &FogEntities, &SignaledEntities](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList      = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FTransformFragment> DetectorTransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassCombatStatsFragment> DetectorStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> DetectorCharacList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const float Now = World->GetTimeSeconds();
        
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];


            // Skip brand‐new units
            if ((Now - StateFrag.BirthTime) < 1.0f)
            {
                continue;
            }

            const FMassEntityHandle DetectorEntity = ChunkContext.GetEntity(i);
            const FMassCombatStatsFragment DetectorStats = DetectorStatsList[i];
            const FMassAgentCharacteristicsFragment DetectorCharacteristics = DetectorCharacList[i];
            const FVector DetectorLocation = DetectorTransformList[i].GetTransform().GetLocation();


            // collect for fog‐of‐war
            FogEntities.Add(DetectorEntity);
            

            // --- STEP 2: test newly signaled entities against active SightRadius
            for (const FMassEntityHandle& SignaledTarget : SignaledEntities)
            {
                if (!EntityManager.IsEntityValid(SignaledTarget)) continue;
                if (SignaledTarget == DetectorEntity) continue;

                
                if (!DoesEntityHaveFragment<FTransformFragment>(EntityManager, SignaledTarget) ||
                !DoesEntityHaveFragment<FMassCombatStatsFragment>(EntityManager, SignaledTarget) ||
                !DoesEntityHaveFragment<FMassAIStateFragment>(EntityManager, SignaledTarget) ||
                !DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EntityManager, SignaledTarget))
                    continue;
                
               const FMassCombatStatsFragment* TargetStats = TryGetFragmentDataPtr<FMassCombatStatsFragment>(EntityManager, SignaledTarget);
                
                if (!TargetStats) continue;

              
                
                if (DetectorStats.TeamId == TargetStats->TeamId) continue;

                // EntityManager.GetFragmentDataChecked
                const FTransformFragment* TargetTransform = TryGetFragmentDataPtr<FTransformFragment>(EntityManager, SignaledTarget);
                if (!TargetTransform) continue;
                FMassAIStateFragment& TargetStateFrag = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(SignaledTarget);
                
                FMassAgentCharacteristicsFragment& TargetCharacteristics = EntityManager.GetFragmentDataChecked<FMassAgentCharacteristicsFragment>(SignaledTarget);

                const float Dist = FVector::Dist(DetectorLocation, TargetTransform->GetTransform().GetLocation());

                
                if (Dist <= DetectorStats.SightRadius) // && TargetStats->Health > 0.0f && DetectorStats.Health > 0.0f
                {
                    // Enter sight
                    if (DetectorCharacteristics.bCanDetectInvisible && TargetCharacteristics.bCanBeInvisible)
                    {
                        int32& DetectorOverlapCount = TargetStateFrag.DetectorOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                        DetectorOverlapCount++;
                    }

                    int32& SightOverlapCount = TargetStateFrag.TeamOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                    SightOverlapCount++;
                    
                }
            }
            

        }

        for (int32 i = 0; i < NumEntities; ++i)
        {

            const FMassEntityHandle DetectorEntity = ChunkContext.GetEntity(i);
            const FMassCombatStatsFragment DetectorStats = DetectorStatsList[i];

            for (const FMassEntityHandle& SignaledTarget : SignaledEntities)
            {
                if (!EntityManager.IsEntityValid(SignaledTarget)) continue;
                if (SignaledTarget == DetectorEntity) continue;
                
                if (!DoesEntityHaveFragment<FTransformFragment>(EntityManager, SignaledTarget) ||
                    !DoesEntityHaveFragment<FMassCombatStatsFragment>(EntityManager, SignaledTarget) ||
                    !DoesEntityHaveFragment<FMassAIStateFragment>(EntityManager, SignaledTarget) ||
                    !DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EntityManager, SignaledTarget))
                                        continue;
  
                const FMassCombatStatsFragment* TargetStats = TryGetFragmentDataPtr<FMassCombatStatsFragment>(EntityManager, SignaledTarget);
                if (!TargetStats) continue;
                if (TargetStats->TeamId == DetectorStats.TeamId) continue;
                
                FMassAIStateFragment& TargetStateFrag = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(SignaledTarget);
                FMassAgentCharacteristicsFragment& TargetCharacteristics = EntityManager.GetFragmentDataChecked<FMassAgentCharacteristicsFragment>(SignaledTarget);
                
                int32& SightOverlapCount = TargetStateFrag.TeamOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                
                if (SightOverlapCount > 0)
                {
                    //UE_LOG(LogTemp, Log, TEXT("SightOverlapCount: %d // TeamId: %d // TargetTeamId: %d"), SightOverlapCount, DetectorStats.TeamId, TargetStats->TeamId);
                    PendingSignals.Emplace(SignaledTarget, DetectorEntity, UnitSignals::UnitEnterSight);
                }else if (SightOverlapCount <= 0)
                {
                    PendingSignals.Emplace(SignaledTarget, DetectorEntity, UnitSignals::UnitExitSight); 
                }

                int32& DetectorOverlapCount = TargetStateFrag.DetectorOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);

                if (DetectorOverlapCount > 0)
                {
                    TargetCharacteristics.bIsInvisible = false;
                }else if (DetectorOverlapCount <= 0 && TargetCharacteristics.bCanBeInvisible)
                {
                    TargetCharacteristics.bIsInvisible = true;
                }

            }
        }
        
    });
    
    if (SignalSubsystem && FogEntities.Num() > 0)
    {
        SignalSubsystem->SignalEntities(UnitSignals::UpdateFogMask, FogEntities);
    }else
    {
        UE_LOG(LogTemp, Error, TEXT("No Entities Found!"));
    }
    // Dispatch signals
    if (!PendingSignals.IsEmpty())
    {
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSightSignalPayload& Payload : SignalsToSend)
                    {
                        if (!Payload.SignalName.IsNone())
                        {
                           StrongSignalSubsystem->SignalEntities(Payload.SignalName, {Payload.TargetEntity, Payload.DetectorEntity});
                        }
                    }
                }
            });
        }
    }

    // CRITICAL: Consume the signals after processing them for this tick.
    if (SignaledEntitiesPtr) // Check if the key existed before trying to remove
    {
        ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    }
}
*/