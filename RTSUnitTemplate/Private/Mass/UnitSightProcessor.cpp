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

    // 3. Pull in the raw detection‐range signals this tick:
    TArray<FMassEntityHandle>* SignaledEntitiesPtr = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange);
    static const TArray<FMassEntityHandle> SEmptyEntityArray;
    const TArray<FMassEntityHandle>& SignaledEntities = (SignaledEntitiesPtr && !SignaledEntitiesPtr->IsEmpty())
        ? *SignaledEntitiesPtr
        : SEmptyEntityArray;

    // 4. Prepare our per‐frame containers:
    TMap<FMassEntityHandle, TSet<FMassEntityHandle>> CurrentFrameSights;
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
    [this, &PendingSignals, &EntityManager, &CurrentFrameSights, &FogEntities, &SignaledEntities](FMassExecutionContext& ChunkContext)
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
                
                const FMassCombatStatsFragment* TargetStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(SignaledTarget);
                if (!TargetStats) continue;
                
                if (DetectorStats.TeamId == TargetStats->TeamId) continue;

                const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(SignaledTarget);
                if (!TargetTransform) continue;
                FMassAIStateFragment& TargetStateFrag = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(SignaledTarget);
                
                FMassAgentCharacteristicsFragment& TargetCharacteristics = EntityManager.GetFragmentDataChecked<FMassAgentCharacteristicsFragment>(SignaledTarget);

                const float Dist = FVector::Dist(DetectorLocation, TargetTransform->GetTransform().GetLocation());
                if (Dist <= DetectorStats.SightRadius)
                {
          
                    // Enter sight
                    if (DetectorCharacteristics.bCanDetectInvisible && TargetCharacteristics.bIsInvisible)
                    {
                        TargetCharacteristics.bIsInvisible = false;
                        int32& DetectorOverlapCount = TargetStateFrag.DetectorOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                        DetectorOverlapCount++;
                    }

                    int32& SightOverlapCount = TargetStateFrag.TeamOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                    SightOverlapCount++;

                   // PendingSignals.Emplace(SignaledTarget, DetectorEntity, UnitSignals::UnitEnterSight);
                    
                }else if (Dist > DetectorStats.LoseSightRadius)
                {
                    /*
                    int32& SightOverlapCount = TargetStateFrag.TeamOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                    if (SightOverlapCount >= 1)
                    {
                        SightOverlapCount--;
                    }
                    else
                    {
                        SightOverlapCount = 0;
                    }

                    int32& DetectorOverlapCount = TargetStateFrag.DetectorOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                    if (DetectorCharacteristics.bCanDetectInvisible && DetectorOverlapCount > 0)
                    {
                        if (DetectorOverlapCount >= 1)
                        {
                            DetectorOverlapCount--;
                        }
                        else
                        {
                            DetectorOverlapCount = 0;
                        }

                        if (DetectorOverlapCount <= 0)
                        {
                            TargetCharacteristics.bIsInvisible = true;
                        }
                    }
                    */
                }
            }
            

        }

        for (int32 i = 0; i < NumEntities; ++i)
        {

            const FMassEntityHandle DetectorEntity = ChunkContext.GetEntity(i);
            const FMassCombatStatsFragment DetectorStats = DetectorStatsList[i];

            for (const FMassEntityHandle& SignaledTarget : SignaledEntities)
            {
                const FMassCombatStatsFragment* TargetStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(SignaledTarget);
                if (!TargetStats) continue;
                    
                FMassAIStateFragment& TargetStateFrag = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(SignaledTarget);

                int32& SightOverlapCount = TargetStateFrag.TeamOverlapsPerTeam.FindOrAdd(DetectorStats.TeamId);
                //UE_LOG(LogTemp, Log, TEXT("SightOverlapCount: %d"), SightOverlapCount);
                
                if (SightOverlapCount > 0)
                {
                   // UE_LOG(LogTemp, Log, TEXT("Sight is Greater 0 !! %d"), SightOverlapCount);
                    PendingSignals.Emplace(SignaledTarget, DetectorEntity, UnitSignals::UnitEnterSight);
                }else if (SightOverlapCount <= 0)
                {
                    PendingSignals.Emplace(SignaledTarget, DetectorEntity, UnitSignals::UnitExitSight); 
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



/*
void UUnitSightProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        // Noch nicht Zeit, diesen Frame überspringen
        return;
    }
    
    TArray<FMassEntityHandle>* SignaledEntitiesPtr = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange);

    // Use a const reference for safety and potentially minor efficiency.
    // If SignaledEntitiesPtr is null, SignaledEntities will be a reference to an empty array.
    static const TArray<FMassEntityHandle> SEmptyEntityArray;
    const TArray<FMassEntityHandle>& SignaledEntities = (SignaledEntitiesPtr && !SignaledEntitiesPtr->IsEmpty()) ? *SignaledEntitiesPtr : SEmptyEntityArray;

    TArray<FMassSightSignalPayload> PendingSignals;

    TArray<FMassEntityHandle> FogEntities;
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture SignaledEntitiesPtr by value, SignaledEntities by const reference.
        [&PendingSignals, &EntityManager, &FogEntities, &SignaledEntities, SignaledEntitiesPtr, this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

        const float Now = World->GetTimeSeconds();
            
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // This is where LastSeenTargets (a TSet) lives

            if ((Now - StateFrag.BirthTime) < 1.0f)
            {
                continue;
            }
            
            const FMassEntityHandle DetectorEntity = ChunkContext.GetEntity(i);
            FogEntities.Add(DetectorEntity);
            const FTransform& DetectorTransform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& DetectorStatsFrag = StatsList[i];
            const FVector DetectorLocation = DetectorTransform.GetLocation();

            // --- REVISED FOG-OF-WAR LOGIC (using TSet for NextFrameLastSeenSet) ---
            TSet<FMassEntityHandle> NextFrameLastSeenSet;
            // Reserve space if TSet supports it and it's beneficial (TSet typically manages its own capacity based on load factor)
            // For TSet, explicit reserve is less common than TArray, but you can initialize with expected size:
            // TSet<FMassEntityHandle> NextFrameLastSeenSet(StateFrag.LastSeenTargets.Num() + SignaledEntities.Num() / 4); // Heuristic

            const float LoseSightRadiusSq = FMath::Square(DetectorStatsFrag.LoseSightRadius);
            const float SightRadiusSq = FMath::Square(DetectorStatsFrag.SightRadius);
            
            // 1. Process previously seen targets from StateFrag.LastSeenTargets (which is a TSet)

            //UE_LOG(LogTemp, Error, TEXT("BStateFrag.LastSeenTargets.Num(): %d"), StateFrag.LastSeenTargets.Num());
            for (const FMassEntityHandle& PreviousTarget : StateFrag.LastSeenTargets)
            {
                if (!EntityManager.IsEntityValid(PreviousTarget)) continue;
                const FTransformFragment* TargetTFPtr = EntityManager.GetFragmentDataPtr<FTransformFragment>(PreviousTarget);
                if (!TargetTFPtr) continue;

                const float DistSq = FVector::DistSquared(DetectorLocation, TargetTFPtr->GetTransform().GetLocation());

                if (DistSq <= LoseSightRadiusSq)
                {
                    // Still within lose sight radius, keep it.
                    NextFrameLastSeenSet.Add(PreviousTarget);
                }
                else
                {
                    // Exited lose sight radius.
                    PendingSignals.Emplace(PreviousTarget, DetectorEntity, UnitSignals::UnitExitSight);
                }
            }

            // 2. Process newly signaled entities (from SignaledEntities, which is a TArray)
            //    Only iterate if SignaledEntities is not empty (guaranteed by the reference setup or check SignaledEntities.IsEmpty())
            if (!SignaledEntities.IsEmpty())
            {
               // UE_LOG(LogTemp, Error, TEXT("SignaledEntities.Num(): %d"), SignaledEntities.Num());
                for (const FMassEntityHandle& SignaledTarget : SignaledEntities)
                {
                    if (SignaledTarget == DetectorEntity) continue;
                    if (!EntityManager.IsEntityValid(SignaledTarget)) continue;

                    const bool bWasAlreadySeenAndKept = NextFrameLastSeenSet.Contains(SignaledTarget);

                    const FTransformFragment* TargetTFPtr = EntityManager.GetFragmentDataPtr<FTransformFragment>(SignaledTarget);
                    if (!TargetTFPtr) continue;

                    const float DistSq = FVector::DistSquared(DetectorLocation, TargetTFPtr->GetTransform().GetLocation());

                    if (DistSq < SightRadiusSq)
                    {
                        // This entity is within active sight radius.
                        if (!bWasAlreadySeenAndKept)
                        {
                            // It wasn't in LastSeenTargets OR it was but fell out of LoseSightRadius (and thus not in NextFrameLastSeenSet yet from step 1),
                            // but now it's within SightRadius. This is an EnterSight event.
                           // UE_LOG(LogTemp, Error, TEXT("EnterSight!"));
                            PendingSignals.Emplace(SignaledTarget, DetectorEntity, UnitSignals::UnitEnterSight);
                        }
                        // Add to the set for next tick. If already present (bWasAlreadySeenAndKept is true), Add will do nothing.
                        NextFrameLastSeenSet.Add(SignaledTarget);
                    }
                    // else: it's in SignaledEntities but not within SightRadius.
                    // If it was previously seen and kept (bWasAlreadySeenAndKept) but now outside SightRadius
                    // (but still within LoseSightRadius), it remains in NextFrameLastSeenSet from step 1.
                    // If it was previously seen but NOT kept (e.g., outside LoseSightRadius), and now it's also outside SightRadius, nothing happens here.
                    // If it's a new entity from SignaledEntities and outside SightRadius, nothing happens.
                }
            }else
            {
                //UE_LOG(LogTemp, Error, TEXT("SignaledEntities.IsEmpty() == TRUE!"));
            }

            // 3. Final update of LastSeenTargets
            StateFrag.LastSeenTargets = MoveTemp(NextFrameLastSeenSet); // Move TSet to TSet
            // --- REVISED FOG-OF-WAR LOGIC END ---
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