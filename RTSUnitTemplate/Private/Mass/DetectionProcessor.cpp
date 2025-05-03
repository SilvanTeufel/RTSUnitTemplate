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
        }
    }
}

// Called by the Signal Subsystem when the corresponding signal is processed
void UDetectionProcessor::HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
    // Append the received entities to our buffer for processing in Execute
    // This function might be called multiple times per frame if signals are dispatched in batches
    ReceivedSignalsBuffer.FindOrAdd(SignalName).Append(Entities.GetData(), Entities.Num());
    // UE_LOG(LogMass, Verbose, TEXT("HandleUnitPresenceSignal: Received %d entities for signal %s"), Entities.Num(), *SignalName.ToString());
}

void UDetectionProcessor::ConfigureQueries()
{
    // Tell the base class which signal we care about:

    // Dieser Prozessor läuft für alle Einheiten, die Ziele erfassen sollen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel schreiben
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Fähigkeiten lesen

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
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);

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

    // Keep track of entities from the buffer that we actually process
    SignaledEntitiesProcessedThisTick.Reset();
    if (SignaledEntitiesPtr)
    {
       SignaledEntitiesProcessedThisTick.Append(SignaledEntities); // Add all potential targets initially
    }
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
            
        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        //UE_LOG(LogTemp, Log, TEXT("Detect Entities: %d"), NumEntities);

        for (int32 i = 0; i < NumEntities; ++i)
        {
           
            const FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);
            const FTransform& CurrentDetectorTransform = TransformList[i].GetTransform();
            FMassAITargetFragment& CurrentTarget = TargetList[i];
            const FMassCombatStatsFragment& CurrentDetectorStats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CurrentDetectorChars = CharList[i];

            const FVector CurrentDetectorLocation = CurrentDetectorTransform.GetLocation();
            const float SightRadiusSq = FMath::Square(CurrentDetectorStats.SightRadius);
            const float LoseSightRadiusSq = FMath::Square(CurrentDetectorStats.LoseSightRadius);

            FMassEntityHandle BestTargetEntity;
            float MinDistSq = SightRadiusSq; // Start search within sight radius
            FVector BestTargetLocation = FVector::ZeroVector;
            bool bFoundValidTargetThisRun = false; // Did we find *any* valid target in the signaled list?
            bool bCurrentTargetStillViable = false; // Is the *existing* target still valid?
            FVector CurrentTargetLatestLocation = FVector::ZeroVector;

            // --- Process ALL Buffered Signals (Manual Filtering) ---
            for (const FMassEntityHandle& PotentialTargetEntity : SignaledEntities)
            {
                // Basic Filtering
                if (PotentialTargetEntity == CurrentEntity) continue;

                const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(PotentialTargetEntity);
                const FMassCombatStatsFragment* TargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(PotentialTargetEntity);
                const FMassAgentCharacteristicsFragment* TargetCharsFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(PotentialTargetEntity);

                if (!TargetTransformFrag || !TargetStatsFrag || !TargetCharsFrag)
                {
                     SignaledEntitiesProcessedThisTick.Remove(PotentialTargetEntity); // Remove incomplete from processed set
                     continue; // Missing required components
                }
                // --- End Data Fetching ---

                const FTransform& TargetTransform = TargetTransformFrag->GetTransform();
                const FVector TargetLocation = TargetTransform.GetLocation();
                const FMassCombatStatsFragment& TargetStats = *TargetStatsFrag;
                const FMassAgentCharacteristicsFragment& TargetChars = *TargetCharsFrag;

                // --- Perform Checks ---
                if (TargetStats.TeamId == CurrentDetectorStats.TeamId) continue;
                if (TargetChars.bIsInvisible && !CurrentDetectorChars.bCanDetectInvisible) continue;
                if (TargetStats.Health <= 0) continue;
             
                
                bool bCanAttackTarget = true;
                if (CurrentDetectorChars.bCanOnlyAttackGround && TargetChars.bIsFlying) bCanAttackTarget = false;
                else if (CurrentDetectorChars.bCanOnlyAttackFlying && !TargetChars.bIsFlying) bCanAttackTarget = false;
                if (!bCanAttackTarget) continue;

                
                // MANUAL DISTANCE CHECK (Spatial Filtering)
                const float DistSq = FVector::DistSquared(CurrentDetectorLocation, TargetLocation);
                if (DistSq > SightRadiusSq) continue; // Outside sight radius

                // TODO: Implement Line-of-Sight Check here
                // --- Update Best Target Logic ---
                if (DistSq < MinDistSq)
                {
                    MinDistSq = DistSq;
                    BestTargetEntity = PotentialTargetEntity;
                    BestTargetLocation = TargetLocation;
                    bFoundValidTargetThisRun = true;
                }
                
                // --- Check Current Target Viability Logic ---
                if (CurrentTarget.TargetEntity.IsSet() && PotentialTargetEntity == CurrentTarget.TargetEntity)
                {
                    if (DistSq <= LoseSightRadiusSq) // Check against LOSE sight radius
                    {
                        // TODO: Check Line of Sight again if required
                        bCurrentTargetStillViable = true;
                        CurrentTargetLatestLocation = TargetLocation;
                    }
                     // If it's the current target but outside lose radius, bCurrentTargetStillViable remains false
                }

            } // End loop through signaled entities
            
            // --- Target Loss Check for Existing Target (if it wasn't in the signal list or was filtered out) ---
            const bool bHadValidTargetPreviously = CurrentTarget.bHasValidTarget && CurrentTarget.TargetEntity.IsSet();
            if (bHadValidTargetPreviously && !bCurrentTargetStillViable)
            {
                // Explicitly check if the current target entity was processed (i.e., signaled and alive/valid)
                // If it wasn't processed OR if it was processed but failed the viability check (e.g., > LoseSightRadiusSq)
                if (!SignaledEntitiesProcessedThisTick.Contains(CurrentTarget.TargetEntity))
                {
                    //UE_LOG(LogTemp, Log, TEXT("Target wasn't in the signals list OR was dead/invalid - definitely lost!!!!"));
                    // Target wasn't in the signals list OR was dead/invalid - definitely lost.
                    // bCurrentTargetStillViable is already false.
                    // UE_LOG(LogMass, Verbose, TEXT("Entity %s lost target %s (not signaled/invalid)."), *CurrentEntity.ToString(), *CurrentTarget.TargetEntity.ToString());
                }
                 else
                 {
                     //UE_LOG(LogTemp, Log, TEXT("Target *was* signaled and valid, but failed the viability check (distance/LoS)"));
                     // Target *was* signaled and valid, but failed the viability check (distance/LoS)
                     // bCurrentTargetStillViable is already false.
                     // UE_LOG(LogMass, Verbose, TEXT("Entity %s lost target %s (out of range/LoS)."), *CurrentEntity.ToString(), *CurrentTarget.TargetEntity.ToString());
                 }
            }


            // --- Final Update Target Fragment Logic ---
            if (bFoundValidTargetThisRun)
            {
                //UE_LOG(LogTemp, Log, TEXT("Found a new best target this run. Prioritize it."));
                // Found a new best target this run. Prioritize it.
                CurrentTarget.TargetEntity = BestTargetEntity;
                CurrentTarget.LastKnownLocation = BestTargetLocation;
                CurrentTarget.bHasValidTarget = true;
            }
            else if (bHadValidTargetPreviously && bCurrentTargetStillViable)
            {
                //UE_LOG(LogTemp, Log, TEXT("No *new* target found, but the *previous* one is still viable. Keep it."));
                // No *new* target found, but the *previous* one is still viable. Keep it.
                CurrentTarget.LastKnownLocation = CurrentTargetLatestLocation; // Update location
                CurrentTarget.bHasValidTarget = true;
            }
            else
            {
                //UE_LOG(LogTemp, Log, TEXT("No new target found AND (no previous target OR previous target is no longer viable). Clear target."));
                // No new target found AND (no previous target OR previous target is no longer viable). Clear target.
                if (CurrentTarget.bHasValidTarget) // Log only if we had one before
                {
                     // UE_LOG(LogMass, Verbose, TEXT("Entity %s clearing target."), *CurrentEntity.ToString());
                }
                CurrentTarget.TargetEntity.Reset();
                CurrentTarget.bHasValidTarget = false;
            }


            // --- Tag Management (using final state) ---
             if (CurrentTarget.bHasValidTarget)
             {
                    //UE_LOG(LogTemp, Log, TEXT("Detection TO CHASE!!!!!!!"));

                       UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                       if (!SignalSubsystem) continue;
                        
                       SignalSubsystem->SignalEntity(UnitSignals::SetUnitToChase, CurrentEntity);
                       SignalSubsystem->SignalEntity(UnitSignals::Chase, CurrentEntity);
             }

        } // End loop through detector entities in chunk
    }); // End ForEachEntityChunk

    // Clear the buffer for this signal AFTER processing all detectors
    ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    SignaledEntitiesProcessedThisTick.Empty(); // Clear the processed set
}