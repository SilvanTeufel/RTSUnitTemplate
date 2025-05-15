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
    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSightProcessor::HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
    // Append the received entities to our buffer for processing in Execute
    // This function might be called multiple times per frame if signals are dispatched in batches
    ReceivedSignalsBuffer.FindOrAdd(SignalName).Append(Entities.GetData(), Entities.Num());
    // UE_LOG(LogMass, Verbose, TEXT("HandleUnitPresenceSignal: Received %d entities for signal %s"), Entities.Num(), *SignalName.ToString());
}



// Presuming SEmptyEntityArray is defined appropriately, e.g.,
// static const TArray<FMassEntityHandle> SEmptyEntityArray;
// in your class or a relevant utility header.
// If not, you can use TArray<FMassEntityHandle>() directly where SEmptyEntityArray was used,
// though a static const is slightly more efficient if that path is hit very frequently.

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

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture SignaledEntitiesPtr by value, SignaledEntities by const reference.
        [&PendingSignals, &EntityManager, &SignaledEntities, SignaledEntitiesPtr, this](FMassExecutionContext& ChunkContext)
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
    TArray<FMassEntityHandle>* SignaledEntitiesPtr = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange);
    if (!SignaledEntitiesPtr || SignaledEntitiesPtr->IsEmpty())
    {
        // No presence signals received since last tick.
        // We MUST still run the target loss logic for existing targets.
        // Fall through to ForEachEntityChunk, but the inner loop over signaled entities won't run.
    }

    // Use a const reference for safety and potentially minor efficiency
    const TArray<FMassEntityHandle>& SignaledEntities = (SignaledEntitiesPtr) ? *SignaledEntitiesPtr : TArray<FMassEntityHandle>{}; // Empty array if ptr is null

    TArray<FMassSightSignalPayload> PendingSignals;
    // Keep track of entities from the buffer that we actually process
    SignaledEntitiesProcessedThisTick.Reset();
    if (SignaledEntitiesPtr)
    {
        SignaledEntitiesProcessedThisTick.Append(SignaledEntities); // Add all potential targets initially
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&PendingSignals, &EntityManager, SignaledEntities, this](FMassExecutionContext& ChunkContext)
    {
             const int32 NumEntities = ChunkContext.GetNumEntities();
             
            auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
            const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
            const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

            //UE_LOG(LogTemp, Log, TEXT("UUnitSightProcessor Entities: %d"), NumEntities);
   
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            
            const float Now = World->GetTimeSeconds();
              if ((Now - StateFrag.BirthTime) < 1.0f)
              {
                  continue;  
              }
     
            const FMassEntityHandle DetectorEntity = ChunkContext.GetEntity(i);
            const FTransform& DetectorTransform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& DetectorStatsFrag = StatsList[i];
            const FVector DetectorLocation = DetectorTransform.GetLocation();

            // --- YOUR FOG-OF-WAR OVERLAP REBUILD START ---
            {
                TSet<FMassEntityHandle> NewLastSeen;

                //UE_LOG(LogTemp, Log, TEXT("DetectorStatsFrag.LoseSightRadius: %f"), DetectorStatsFrag.LoseSightRadius);
                //UE_LOG(LogTemp, Log, TEXT("DetectorStatsFrag.SightRadius: %f"), DetectorStatsFrag.SightRadius);
                // 1) First, gather all entities we *could* still consider seen:
                for (const FMassEntityHandle& T : StateFrag.LastSeenTargets)
                {
                    if (!EntityManager.IsEntityValid(T)) continue;
                    const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(T);
                    if (!TF) continue;
                    const float Dist = FVector::Dist(DetectorLocation, TF->GetTransform().GetLocation());
                    if (Dist <= DetectorStatsFrag.LoseSightRadius)
                    {
                        NewLastSeen.Add(T);
                    }
                }
    
                // 2) Then, also add any *newly* seen inside SightRadius
                for (const FMassEntityHandle& T : SignaledEntities)
                {
                    if (T == DetectorEntity) continue;
                    if (!EntityManager.IsEntityValid(T)) continue;
                    const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(T);
                    if (!TF) continue;
                    const float Dist = FVector::Dist(DetectorLocation, TF->GetTransform().GetLocation());
                    if (Dist < DetectorStatsFrag.SightRadius)
                    {
                        if (!NewLastSeen.Contains(T))
                        {
                            // EnterSight
                            PendingSignals.Emplace(T,DetectorEntity, UnitSignals::UnitEnterSight);
                        }
                        NewLastSeen.Add(T);
                    }
                }

           
                // 3) Anyone in the *old* LastSeenTargets but not in our NewLastSeen set has now truly exited
                for (const FMassEntityHandle& T : StateFrag.LastSeenTargets)
                {
                    if (!NewLastSeen.Contains(T))
                    {
                        PendingSignals.Emplace(T,DetectorEntity, UnitSignals::UnitExitSight);
                    }
                }

                // 4) Store for next tick
                StateFrag.LastSeenTargets = MoveTemp(NewLastSeen);
            }
            // --- YOUR FOG-OF-WAR OVERLAP REBUILD END ---
        }
    });

    // Dispatch signals
    if (!PendingSignals.IsEmpty())
    {
        if (SignalSubsystem)
        {
            TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
            // Capture the weak subsystem pointer and move the pending signals list
            AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
            {
                // Check if the subsystem is still valid on the Game Thread
                if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
                {
                    for (const FMassSightSignalPayload& Payload : SignalsToSend)
                    {
                        // Check if the FName is valid before sending
                        if (!Payload.SignalName.IsNone())
                        {
                           // Send signal safely from the Game Thread using FName
                           StrongSignalSubsystem->SignalEntities(Payload.SignalName, {Payload.TargetEntity, Payload.DetectorEntity});
                        }
                    }
                }
            });
        }
    }

    //ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    SignaledEntitiesProcessedThisTick.Empty(); // Clear the processed set
}
*/