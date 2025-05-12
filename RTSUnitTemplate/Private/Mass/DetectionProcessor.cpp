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
#include "Mass/Signals/UnitSignalingProcessor.h"


UDetectionProcessor::UDetectionProcessor()
{
    // Sollte vor der State Machine laufen
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UDetectionProcessor::Initialize(UObject& Owner)
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
                                      .AddUObject(this, &UDetectionProcessor::HandleUnitPresenceSignal);
            //UE_LOG(LogMass, Log, TEXT("UDetectionProcessor bound to signal '%s'"), *UnitSignals::UnitInDetectionRange.ToString());

            if (SpawnSignalDelegateHandle.IsValid())
            {
                SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitSpawned).Remove(SpawnSignalDelegateHandle);
            }
            
            SpawnSignalDelegateHandle = SignalSubsystem
                   ->GetSignalDelegateByName(UnitSignals::UnitSpawned)
                   .AddUObject(this, &UDetectionProcessor::HandleUnitSpawnedSignal);
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

    if (SignalSubsystem && SpawnSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
    {
        SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitSpawned)
        .Remove(SpawnSignalDelegateHandle);
            
        SpawnSignalDelegateHandle.Reset();
    }

    Super::BeginDestroy();
}

// Called by the Signal Subsystem when the corresponding signal is processed
void UDetectionProcessor::HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
    // Append the received entities to our buffer for processing in Execute
    // This function might be called multiple times per frame if signals are dispatched in batches
    ReceivedSignalsBuffer.FindOrAdd(SignalName).Append(Entities.GetData(), Entities.Num());
    // UE_LOG(LogMass, Verbose, TEXT("HandleUnitPresenceSignal: Received %d entities for signal %s"), Entities.Num(), *SignalName.ToString());
}

void UDetectionProcessor::HandleUnitSpawnedSignal(
    FName SignalName,
    TConstArrayView<FMassEntityHandle> Entities)
{
    const float Now = World->GetTimeSeconds();

    if (!EntitySubsystem) { return; }
    
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
    for (const FMassEntityHandle& E : Entities)
    {
       ;
        // Grab the *target* fragment on that entity and stamp it
        FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(E);
        
        if (StateFragment)
        {
            StateFragment->BirthTime = Now;
        }
    }
}

void UDetectionProcessor::ConfigureQueries()
{
    // Tell the base class which signal we care about:

    // Dieser Prozessor läuft für alle Einheiten, die Ziele erfassen sollen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel schreiben
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Fähigkeiten lesen
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    // Optional: Nur für bestimmte Zustände laufen lassen?
    EntityQuery.AddTagRequirement<FMassStateDetectTag>(EMassFragmentPresence::All);
    /*
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
    */
    // ...

    // Schließe tote Einheiten aus
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    /*
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
    */
    
    EntityQuery.RegisterWithProcessor(*this);
}



void UDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Log, TEXT("UDetectionProcessor!"));
// Check if we have any signals buffered for the detection signal name
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
        const TArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); 
        //UE_LOG(LogTemp, Log, TEXT("Detect Entities: %d"), NumEntities);
       
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; 
            const float Now = World->GetTimeSeconds();
            if ((Now - StateFrag.BirthTime) < 1.0f /* or 2.0f */)
            {
                continue;  // this entity is not yet “1 second old”
            }
            
            const FMassEntityHandle DetectorEntity = ChunkContext.GetEntity(i);
            const FTransform& DetectorTransform = TransformList[i].GetTransform();
            FMassAITargetFragment& DetectorTargetFrag = TargetList[i];
            const FMassCombatStatsFragment& DetectorStatsFrag = StatsList[i];
            const FMassAgentCharacteristicsFragment& DetectorCharFrag = CharList[i];
            const FVector DetectorLocation = DetectorTransform.GetLocation();
            FMassMoveTargetFragment& MoveTargetFrag = MoveTargetList[i];
     

            FMassEntityHandle BestTargetEntity;
           
            FVector BestTargetLocation = FVector::ZeroVector;
            bool bFoundValidTargetThisRun = false; // Did we find *any* valid target in the signaled list?
            bool bCurrentTargetStillViable = false; // Is the *existing* target still valid?
            FVector CurrentTargetLatestLocation = FVector::ZeroVector;
            

            // --- YOUR FOG-OF-WAR OVERLAP REBUILD START --- ///////////////////////////////////////////////////////////////////////////////////////////
            {
                // 1) Collect who we see *this* tick
                TSet<FMassEntityHandle> CurrentSeenTargets;
                for (const FMassEntityHandle& PotentialTarget : SignaledEntities)
                {
                    if (PotentialTarget == DetectorEntity) continue;
                    if (!EntityManager.IsEntityValid(PotentialTarget)) continue;
                    // distance check (reuse your code’s Dist calculation)
                    const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(PotentialTarget);
                    if (!TF) continue;
                    const float Dist = FVector::Dist(
                        DetectorLocation,
                        TF->GetTransform().GetLocation()
                    );
                    if (Dist < DetectorStatsFrag.SightRadius)
                    {
                        CurrentSeenTargets.Add(PotentialTarget);
                    }
                }

                // 2) Fire “enter” signals for newly-seen
                for (auto& T : CurrentSeenTargets)
                {
                    if (!StateFrag.LastSeenTargets.Contains(T))
                    {
                        PendingSignals.Emplace(T, UnitSignals::UnitEnterSight);
                    }
                }
                // 3) Fire “exit” signals for no-longer-seen
                for (auto& T : StateFrag.LastSeenTargets)
                {
                    if (!CurrentSeenTargets.Contains(T))
                    {
                        PendingSignals.Emplace(T, UnitSignals::UnitExitSight);
                    }
                }
                // 4) Save for next tick
                StateFrag.LastSeenTargets = MoveTemp(CurrentSeenTargets);
            }
            // --- YOUR FOG-OF-WAR OVERLAP REBUILD END --- ///////////////////////////////////////////////////////////////////////////////////////////

            
            // --- Process ALL Buffered Signals (Manual Filtering) ---
            for (const FMassEntityHandle& PotentialTargetEntity : SignaledEntities)
            {
                // Basic Filtering
                if (PotentialTargetEntity == DetectorEntity) continue;
                //if (!DetectorTargetFrag.TargetEntity.IsSet())  continue;

                if (!EntityManager.IsEntityValid(PotentialTargetEntity))
                {
                    SignaledEntitiesProcessedThisTick.Remove(PotentialTargetEntity);
                    continue;
                }

           
                // *Guard each fragment* before dereferencing it
                if (!DoesEntityHaveFragment<FTransformFragment>(EntityManager, PotentialTargetEntity) ||
                    !DoesEntityHaveFragment<FMassCombatStatsFragment>(EntityManager, PotentialTargetEntity) ||
                    !DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EntityManager, PotentialTargetEntity))
                {
                  // skip any entity missing one of the required fragments
                  SignaledEntitiesProcessedThisTick.Remove(PotentialTargetEntity);
                  continue;
                }

                const FMassCombatStatsFragment* TargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(PotentialTargetEntity);
                const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(PotentialTargetEntity);
                const FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(PotentialTargetEntity);
    
                // --- End Data Fetching ---

                const FTransform& TargetTransform = TargetTransformFrag->GetTransform();
                const FVector TargetLocation = TargetTransform.GetLocation();
                const FMassCombatStatsFragment& TargetStats = *TargetStatsFrag;
                const FMassAgentCharacteristicsFragment& TargetChars = *TargetCharFrag;



                
                //UE_LOG(LogTemp, Log, TEXT("AAAA"));
                // --- Perform Checks ---
                if (TargetStats.TeamId == DetectorStatsFrag.TeamId) continue;
             
                
                bool bCanAttackTarget = true;
                if (DetectorCharFrag.bCanOnlyAttackGround && TargetChars.bIsFlying) bCanAttackTarget = false;
                else if (DetectorCharFrag.bCanOnlyAttackFlying && !TargetChars.bIsFlying) bCanAttackTarget = false;
                else if (TargetChars.bIsInvisible && !DetectorCharFrag.bCanDetectInvisible) bCanAttackTarget = false;
                if (!bCanAttackTarget) continue;
                
                const float Dist = FVector::Dist(DetectorLocation, TargetLocation);

                // --- Update Best Target Logic ---
                if (Dist < DetectorStatsFrag.SightRadius && TargetStatsFrag->Health > 0)
                {
                    BestTargetEntity = PotentialTargetEntity;
                    BestTargetLocation = TargetLocation;
                    bFoundValidTargetThisRun = true;
                }
                
                // --- Check Current Target Viability Logic ---
                if (DetectorTargetFrag.TargetEntity.IsSet()) //  && PotentialTargetEntity == DetectorTargetFrag.TargetEntity
                {
                    const FTransformFragment* CurrentTargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(DetectorTargetFrag.TargetEntity);
                    const FTransform& CurrentTargetDetectorTransform = CurrentTargetTransformFrag->GetTransform();
                    const FVector CurrentTargetLocation = CurrentTargetDetectorTransform.GetLocation();
                    const float CurrentDist = FVector::Dist(DetectorLocation, CurrentTargetLocation);
                    const FMassCombatStatsFragment* CurrentTargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(DetectorTargetFrag.TargetEntity);
                    
                    if (CurrentDist <= DetectorStatsFrag.LoseSightRadius && CurrentTargetStatsFrag->Health > 0) // Check against LOSE sight radius
                    {
                        if (!bFoundValidTargetThisRun || Dist >= CurrentDist) {
                             bFoundValidTargetThisRun        = false;
                             bCurrentTargetStillViable       = true;
                             CurrentTargetLatestLocation     = TargetLocation;
                         }
                    }
                     // If it's the current target but outside lose radius, bCurrentTargetStillViable remains false
                }

            } // End loop through signaled entities
            



            // --- Final Update Target Fragment Logic ---
            if (bFoundValidTargetThisRun)
            {
                //UE_LOG(LogTemp, Log, TEXT("NEW TARGET."));
                // Found a new best target this run. Prioritize it.
                DetectorTargetFrag.TargetEntity = BestTargetEntity;
                DetectorTargetFrag.LastKnownLocation = BestTargetLocation;
                DetectorTargetFrag.bHasValidTarget = true;
            }
            else if (bCurrentTargetStillViable) // bHadValidTargetPreviously &&
            {
                //UE_LOG(LogTemp, Log, TEXT("SAME TARGET."));
                DetectorTargetFrag.LastKnownLocation = CurrentTargetLatestLocation; // Update location
                DetectorTargetFrag.bHasValidTarget = true;
            }
            else
            {
                //UE_LOG(LogTemp, Log, TEXT("NO TARGET."));
               // DetectorTargetFrag.TargetEntity.Reset();
               // DetectorTargetFrag.bHasValidTarget = false;
            }


            if (DetectorTargetFrag.TargetEntity.IsSet()) //  && PotentialTargetEntity == DetectorTargetFrag.TargetEntity
            {
                const FTransformFragment* CurrentTargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(DetectorTargetFrag.TargetEntity);
                const FTransform& CurrentTargetDetectorTransform = CurrentTargetTransformFrag->GetTransform();
                const FVector CurrentTargetLocation = CurrentTargetDetectorTransform.GetLocation();
                const float CurrentDist = FVector::Dist(DetectorLocation, CurrentTargetLocation);
                const FMassCombatStatsFragment* CurrentTargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(DetectorTargetFrag.TargetEntity);
                
                if (CurrentDist > DetectorStatsFrag.LoseSightRadius || CurrentTargetStatsFrag->Health <= 0) // Check against LOSE sight radius
                {
                    DetectorTargetFrag.TargetEntity.Reset();
                    DetectorTargetFrag.bHasValidTarget = false;
                    UpdateMoveTarget(MoveTargetFrag, StateFrag.StoredLocation, DetectorStatsFrag.RunSpeed, World);
                }
                 // If it's the current target but outside lose radius, bCurrentTargetStillViable remains false
            }
           

            // --- Tag Management (using final state) ---
            
             if (DetectorTargetFrag.bHasValidTarget)
             {
                 PendingSignals.Emplace(DetectorEntity, UnitSignals::SetUnitToChase);
             }
            
        } // End loop through detector entities in chunk
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
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
    // Clear the buffer for this signal AFTER processing all detectors
    ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    SignaledEntitiesProcessedThisTick.Empty(); // Clear the processed set
}
