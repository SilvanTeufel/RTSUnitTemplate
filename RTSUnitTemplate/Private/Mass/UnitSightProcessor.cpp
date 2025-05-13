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
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
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

    TArray<FMassSignalPayload> PendingSignals;
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
                            PendingSignals.Emplace(T, UnitSignals::UnitEnterSight);
                        }
                        NewLastSeen.Add(T);
                    }
                }

           
                // 3) Anyone in the *old* LastSeenTargets but not in our NewLastSeen set has now truly exited
                for (const FMassEntityHandle& T : StateFrag.LastSeenTargets)
                {
                    if (!NewLastSeen.Contains(T))
                    {
                        PendingSignals.Emplace(T, UnitSignals::UnitExitSight);
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
                    for (const FMassSignalPayload& Payload : SignalsToSend)
                    {
                        // Check if the FName is valid before sending
                        if (!Payload.SignalName.IsNone())
                        {
                           // Send signal safely from the Game Thread using FName
                           StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                        }
                    }
                }
            });
        }
    }

    //ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    SignaledEntitiesProcessedThisTick.Empty(); // Clear the processed set
}
