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

void UDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    // Tell the base class which signal we care about:
    EntityQuery.Initialize(EntityManager);
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

    // 2) Build a flat array of “all my detectors”
    struct FUnitInfo
    {
        FMassEntityHandle                          Entity;
        FVector                                    Location;
        FMassAIStateFragment*                      State;
        FMassAITargetFragment*                     TargetFrag;
        FMassMoveTargetFragment*                   MoveFrag;
        const FMassCombatStatsFragment*            Stats;
        const FMassAgentCharacteristicsFragment*   Char;
    };
    TArray<FUnitInfo> Units;
    Units.Reserve(256);

    EntityQuery.ForEachEntityChunk(Context,
        [&Units, this](FMassExecutionContext& ChunkCtx)
    {
        const int32 N = ChunkCtx.GetNumEntities();
        const auto& Transforms    = ChunkCtx.GetFragmentView<FTransformFragment>();
        auto*        StateList    = ChunkCtx.GetMutableFragmentView<FMassAIStateFragment>().GetData();
        auto*        TargetList   = ChunkCtx.GetMutableFragmentView<FMassAITargetFragment>().GetData();
        auto*        MoveList     = ChunkCtx.GetMutableFragmentView<FMassMoveTargetFragment>().GetData();
        const auto&  StatsList    = ChunkCtx.GetFragmentView<FMassCombatStatsFragment>();
        const auto&  CharList     = ChunkCtx.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < N; ++i)
        {
            // skip too‐young
            const float Age = World->GetTimeSeconds() - StateList[i].BirthTime;
            if (Age < 1.f) 
                continue;

            Units.Add({
                ChunkCtx.GetEntity(i),
                Transforms[i].GetTransform().GetLocation(),
                &StateList[i],
                &TargetList[i],
                &MoveList[i],
                &StatsList[i],
                &CharList[i]
            });
        }
    });

    // 3) For each detector, scan every other unit to pick BestEntity / CurrentStillViable
    TArray<FMassSignalPayload> PendingSignals;
    PendingSignals.Reserve(Units.Num());

    for (auto& Det : Units)
    {
        const float Now = World->GetTimeSeconds();

        FMassEntityHandle BestEntity;
        FVector BestLocation = FVector::ZeroVector;
        bool bFoundNew = false;
        bool bCurrentStillViable = false;
        FVector CurrentLocation = FVector::ZeroVector;
        float ShortestDistSq = FMath::Square(Det.Stats->SightRadius);

        for (auto& Tgt : Units)
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

            const float DistSq = FVector::DistSquared(Det.Location, Tgt.Location);
            // “new” target if in sight radius and closer than anything before, and alive
            if (DistSq < ShortestDistSq && Tgt.Stats->Health > 0)
            {
                BestEntity     = Tgt.Entity;
                BestLocation   = Tgt.Location;
                bFoundNew      = true;
                ShortestDistSq = DistSq;
            }

            // “current” target still viable if it’s the same one and within lose‐sight radius
            if (Tgt.Entity == Det.TargetFrag->TargetEntity &&
                DistSq < FMath::Square(Det.Stats->LoseSightRadius))
            {
                CurrentLocation    = Tgt.Location;
                bCurrentStillViable = true;
            }
        }

        // 4) Update target fragment
        if (bFoundNew)
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
            UpdateMoveTarget(
                *Det.MoveFrag,
                Det.State->StoredLocation,
                Det.Stats->RunSpeed,
                World);
        }

        // 5) Queue a single state‐change signal
        /*
        if (Det.TargetFrag->bHasValidTarget && !Det.State->SwitchingState)
        {
            PendingSignals.Emplace(Det.Entity, UnitSignals::SetUnitToChase);
        }*/
    }

    // 6) Dispatch all signals at once
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
/*

void UDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; // Intervall nicht erreicht, überspringen
    }

    // 3. Signaled Entities aus dem Buffer auslesen
    TArray<FMassEntityHandle>* SignaledEntitiesPtr = ReceivedSignalsBuffer.Find(UnitSignals::UnitInDetectionRange);
    const TArray<FMassEntityHandle> SignaledEntities = SignaledEntitiesPtr ? *SignaledEntitiesPtr : TArray<FMassEntityHandle>{};

    // --- Pre-fetch fragment data für signalisierte Entities ---
    struct FCachedSignalData
    {
        FMassEntityHandle Entity;
        FTransform         Transform;
        FMassCombatStatsFragment      CombatStats;
        FMassAgentCharacteristicsFragment Char;
        bool              bValid = false;
    };

    TArray<FCachedSignalData> CachedSignals;
    CachedSignals.Reserve(SignaledEntities.Num());

    for (const FMassEntityHandle& Handle : SignaledEntities)
    {
        FCachedSignalData Data;
        Data.Entity = Handle;

        if (!EntityManager.IsEntityValid(Handle))
        {
            continue;
        }

        // Prüfen, ob alle benötigten Fragmente vorhanden sind
        if (DoesEntityHaveFragment<FTransformFragment>(EntityManager, Handle) &&
            DoesEntityHaveFragment<FMassCombatStatsFragment>(EntityManager, Handle) &&
            DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EntityManager, Handle))
        {
            // Fragmente holen und in Data cachen
            const FTransformFragment* Tf  = EntityManager.GetFragmentDataPtr<FTransformFragment>(Handle);
            const FMassCombatStatsFragment* Stats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Handle);
            const FMassAgentCharacteristicsFragment* Ch = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Handle);

            Data.Transform     = Tf->GetTransform();
            Data.CombatStats   = *Stats;
            Data.Char          = *Ch;
            Data.bValid        = true;
        }

        CachedSignals.Add(Data);
    }

    // 4. Für jedes Chunk der eigenen Entities Analysieren
    TArray<FMassSignalPayload> PendingSignals;
    SignaledEntitiesProcessedThisTick.Reset();
    if (SignaledEntitiesPtr)
    {
        SignaledEntitiesProcessedThisTick.Append(SignaledEntities);
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&PendingSignals, &EntityManager, &CachedSignals, this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList   = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto& TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto TargetList  = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const auto& StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto& CharList  = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        auto MoveTargetList   = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& DetState = StateList[i];
            const float Now = World->GetTimeSeconds();
            if (Now - DetState.BirthTime < 1.0f)
            {
                continue; // Entity noch zu jung
            }

            const FMassEntityHandle SelfEntity = ChunkContext.GetEntity(i);
            const FVector DetectorLocation = TransformList[i].GetTransform().GetLocation();
            FMassAITargetFragment& DetTarget = TargetList[i];
            const FMassCombatStatsFragment& DetStats = StatsList[i];
            const FMassAgentCharacteristicsFragment& DetChar = CharList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            // Tracking Best/New Target
            FMassEntityHandle BestEntity;
            FVector BestLocation = FVector::ZeroVector;
            bool bFoundNew = false;
            bool bCurrentStillViable = false;
            FVector CurrentLocation = FVector::ZeroVector;

            float ShortestDistance = DetStats.SightRadius;
            // Durch alle gecachten Signale iterieren
            for (const FCachedSignalData& Sig : CachedSignals)
            {
                if (!Sig.bValid || Sig.Entity == SelfEntity)
                {
                    continue;
                }

                // Teams und Charakteristika prüfen
                if (Sig.CombatStats.TeamId == DetStats.TeamId)
                {
                    continue;
                }
                bool bCanAttack = true;
                if (DetChar.bCanOnlyAttackGround && Sig.Char.bIsFlying)      bCanAttack = false;
                if (DetChar.bCanOnlyAttackFlying && !Sig.Char.bIsFlying)     bCanAttack = false;
                if (Sig.Char.bIsInvisible && !DetChar.bCanDetectInvisible)   bCanAttack = false;
                if (!bCanAttack) continue;

                const float Dist = FVector::Dist(DetectorLocation, Sig.Transform.GetLocation());
                // Neue Ziele entdecken
                if ( Dist < ShortestDistance && Dist < DetStats.SightRadius && Sig.CombatStats.Health > 0)
                {
                    BestEntity   = Sig.Entity;
                    BestLocation = Sig.Transform.GetLocation();
                    bFoundNew    = true;
                    ShortestDistance = Dist;
                }
                
                // Aktuelles Ziel aufrechterhalten?
                if (DetTarget.TargetEntity.IsSet() && IsFullyValidTarget(EntityManager, DetTarget.TargetEntity))
                {
                    const FTransformFragment* CurrTf = EntityManager.GetFragmentDataPtr<FTransformFragment>(DetTarget.TargetEntity);
                    const FMassCombatStatsFragment* CurrStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(DetTarget.TargetEntity);

                     if (CurrTf != nullptr || CurrStats != nullptr) {
                        FVector CurrLoc = CurrTf->GetTransform().GetLocation();
                        const float CurrDist = FVector::Dist(DetectorLocation, CurrLoc);
                        if (CurrDist <= DetStats.LoseSightRadius && CurrStats->Health > 0 && !bFoundNew)
                        {
                            bCurrentStillViable = true;
                            CurrentLocation = CurrLoc;
                        }
                    }
                }
            }

            // Fragment-Update basierend auf Funden
            if (bFoundNew)
            {
                DetTarget.TargetEntity      = BestEntity;
                DetTarget.LastKnownLocation = BestLocation;
                DetTarget.bHasValidTarget   = true;
            }
            else if (bCurrentStillViable)
            {
                DetTarget.LastKnownLocation = CurrentLocation;
                DetTarget.bHasValidTarget   = true;
            }
            else
            {
                // kein Ziel, ggf. MoveTarget zurücksetzen
            }

            // Verlust prüfen
            if (DetTarget.TargetEntity.IsSet() && IsFullyValidTarget(EntityManager, DetTarget.TargetEntity))
            {
                const FTransformFragment* CurrTf = EntityManager.GetFragmentDataPtr<FTransformFragment>(DetTarget.TargetEntity);
                const FMassCombatStatsFragment* CurrStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(DetTarget.TargetEntity);
                if (CurrTf == nullptr || CurrStats == nullptr)
                {
                    DetTarget.TargetEntity.Reset();
                    DetTarget.bHasValidTarget = false;
                    UpdateMoveTarget(MoveTarget, DetState.StoredLocation, DetStats.RunSpeed, World);
                }else
                {
                    const FVector CurrLoc = CurrTf->GetTransform().GetLocation();
                    const float Dist = FVector::Dist(DetectorLocation, CurrLoc);
                    if (Dist > DetStats.LoseSightRadius || CurrStats->Health <= 0)
                    {
                        DetTarget.TargetEntity.Reset();
                        DetTarget.bHasValidTarget = false;
                        UpdateMoveTarget(MoveTarget, DetState.StoredLocation, DetStats.RunSpeed, World);
                    }
                }
            }

            // Signale zum State Wechsel
            if (DetTarget.bHasValidTarget && !DetState.SwitchingState)
            {
                PendingSignals.Emplace(SelfEntity, UnitSignals::SetUnitToChase);
            }
            else if (!DetState.SwitchingState)
            {
                //PendingSignals.Emplace(SelfEntity, UnitSignals::SetUnitStatePlaceholder);
            }
        }
    });

    // 5. Ausgehende Signale senden
    if (!PendingSignals.IsEmpty() && SignalSubsystem)
    {
        TWeakObjectPtr<UMassSignalSubsystem> SubPtr = SignalSubsystem;
        AsyncTask(ENamedThreads::GameThread, [SubPtr, Signals = MoveTemp(PendingSignals)]()
        {
            if (UMassSignalSubsystem* Sub = SubPtr.Get())
            {
                for (const FMassSignalPayload& P : Signals)
                {
                    if (!P.SignalName.IsNone())
                    {
                        Sub->SignalEntity(P.SignalName, P.TargetEntity);
                    }
                }
            }
        });
    }

    // 6. Cleanup
    ReceivedSignalsBuffer.Remove(UnitSignals::UnitInDetectionRange);
    SignaledEntitiesProcessedThisTick.Empty();
}
*/

/*
void UDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        // Noch nicht Zeit, diesen Frame überspringen
        return;
    }
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
            FMassAIStateFragment& DetectorStateFrag = StateList[i]; 
            const float Now = World->GetTimeSeconds();
            if ((Now - DetectorStateFrag.BirthTime) < 1.0f)
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
                
                const FMassCombatStatsFragment* TargetStatsFrag = TryGetFragmentDataPtr<FMassCombatStatsFragment>(EntityManager, PotentialTargetEntity);
                const FTransformFragment* TargetTransformFrag = TryGetFragmentDataPtr<FTransformFragment>(EntityManager,PotentialTargetEntity);
                const FMassAgentCharacteristicsFragment* TargetCharFrag = TryGetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityManager,PotentialTargetEntity);
    
                // --- End Data Fetching ---

                const FTransform& TargetTransform = TargetTransformFrag->GetTransform();
                const FVector TargetLocation = TargetTransform.GetLocation();
                const FMassCombatStatsFragment& TargetStats = *TargetStatsFrag;
                const FMassAgentCharacteristicsFragment& TargetChars = *TargetCharFrag;
                
                // --- Perform Checks ---
                if (TargetStats.TeamId == DetectorStatsFrag.TeamId) continue;
                
                
                bool bCanAttackTarget = true;
                if (DetectorCharFrag.bCanOnlyAttackGround && TargetChars.bIsFlying)
                {
                    //UE_LOG(LogTemp, Log, TEXT("DetectorCharFrag.bCanOnlyAttackGround %d"), DetectorCharFrag.bCanOnlyAttackGround);
                    //UE_LOG(LogTemp, Log, TEXT("TargetChars.bIsFlying %d"), TargetChars.bIsFlying);
                    bCanAttackTarget = false;
                }
                else if (DetectorCharFrag.bCanOnlyAttackFlying && !TargetChars.bIsFlying)
                {
                    //UE_LOG(LogTemp, Log, TEXT("DetectorCharFrag.bCanOnlyAttackFlying %d"), DetectorCharFrag.bCanOnlyAttackFlying);
                    //UE_LOG(LogTemp, Log, TEXT("TargetChars.bIsFlying %d"), TargetChars.bIsFlying);
                    bCanAttackTarget = false;
                }
                else if (TargetChars.bIsInvisible && !DetectorCharFrag.bCanDetectInvisible)
                {
                    //UE_LOG(LogTemp, Log, TEXT("DetectorCharFrag.bCanDetectInvisible %d"), DetectorCharFrag.bCanDetectInvisible);
                    //UE_LOG(LogTemp, Log, TEXT("TargetChars.bIsInvisible %d"), TargetChars.bIsInvisible);
                    bCanAttackTarget = false;
                }
                if (!bCanAttackTarget) continue;
                
                const float Dist = FVector::Dist(DetectorLocation, TargetLocation);
                
                // --- Update Best Target Logic ---
                if (Dist < DetectorStatsFrag.SightRadius && TargetStatsFrag->Health > 0)
                {
                    BestTargetEntity = PotentialTargetEntity;
                    BestTargetLocation = TargetLocation;
                    bFoundValidTargetThisRun = true;
                    //UE_LOG(LogTemp, Log, TEXT("FOUND TARGET!"));
                }
                
                // --- Check Current Target Viability Logic ---
                if (DetectorTargetFrag.TargetEntity.IsSet() && IsFullyValidTarget(EntityManager, DetectorTargetFrag.TargetEntity)) //  && PotentialTargetEntity == DetectorTargetFrag.TargetEntity
                {
                    const FTransformFragment* CurrentTargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(DetectorTargetFrag.TargetEntity);
                    const FTransform& CurrentTargetDetectorTransform = CurrentTargetTransformFrag->GetTransform();
                    const FVector CurrentTargetLocation = CurrentTargetDetectorTransform.GetLocation();
                    const float CurrentDist = FVector::Dist(DetectorLocation, CurrentTargetLocation);
                    const FMassCombatStatsFragment* CurrentTargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(DetectorTargetFrag.TargetEntity);
                    
                    if (CurrentDist <= DetectorStatsFrag.LoseSightRadius && CurrentTargetStatsFrag->Health > 0) // Check against LOSE sight radius
                    {
                        if (!bFoundValidTargetThisRun) { // || Dist >= CurrentDist
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
                // UE_LOG(LogTemp, Log, TEXT("SAME TARGET."));
                DetectorTargetFrag.LastKnownLocation = CurrentTargetLatestLocation; // Update location
                DetectorTargetFrag.bHasValidTarget = true;
            }
            else
            {
                //UE_LOG(LogTemp, Log, TEXT("NO TARGET."));
                //UpdateMoveTarget(MoveTargetFrag, DetectorStateFrag.StoredLocation, DetectorStatsFrag.RunSpeed, World);
            }

            if (DetectorTargetFrag.TargetEntity.IsSet() && IsFullyValidTarget(EntityManager, DetectorTargetFrag.TargetEntity)) //  && PotentialTargetEntity == DetectorTargetFrag.TargetEntity
            {
                const FTransformFragment* CurrentTargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(DetectorTargetFrag.TargetEntity);
                const FTransform& CurrentTargetDetectorTransform = CurrentTargetTransformFrag->GetTransform();
                const FVector CurrentTargetLocation = CurrentTargetDetectorTransform.GetLocation();
                const float CurrentDist = FVector::Dist(DetectorLocation, CurrentTargetLocation);
                const FMassCombatStatsFragment* CurrentTargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(DetectorTargetFrag.TargetEntity);
                //UE_LOG(LogTemp, Log, TEXT("LoseSightCHECK."));
                if (CurrentDist > DetectorStatsFrag.LoseSightRadius || CurrentTargetStatsFrag->Health <= 0) // Check against LOSE sight radius
                {
                    DetectorTargetFrag.TargetEntity.Reset();
                    DetectorTargetFrag.bHasValidTarget = false;
                    UpdateMoveTarget(MoveTargetFrag, DetectorStateFrag.StoredLocation, DetectorStatsFrag.RunSpeed, World);
                }
                 // If it's the current target but outside lose radius, bCurrentTargetStillViable remains false
            }
           

            // --- Tag Management (using final state) ---
            
             if (DetectorTargetFrag.bHasValidTarget && !DetectorStateFrag.SwitchingState)
             {
                 PendingSignals.Emplace(DetectorEntity, UnitSignals::SetUnitToChase);
             }else if (!DetectorStateFrag.SwitchingState &&
             !DoesEntityHaveTag(EntityManager, DetectorEntity, FMassStatePatrolTag::StaticStruct()) &&
                 !DoesEntityHaveTag(EntityManager, DetectorEntity, FMassStatePatrolRandomTag::StaticStruct()) &&
                !DoesEntityHaveTag(EntityManager, DetectorEntity, FMassStatePatrolIdleTag::StaticStruct()) &&
                !DoesEntityHaveTag(EntityManager, DetectorEntity, FMassStateIdleTag::StaticStruct()) &&
                !DoesEntityHaveTag(EntityManager, DetectorEntity, FMassStateRunTag::StaticStruct()))
             {
                 PendingSignals.Emplace(DetectorEntity, UnitSignals::SetUnitStatePlaceholder);
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
*/