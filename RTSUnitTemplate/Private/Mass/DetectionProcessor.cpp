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
    SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>();
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
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);
    // ...

    // Schließe tote Einheiten aus
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);


    EntityQuery.RegisterWithProcessor(*this);
}

void UDetectionProcessor::SignalEntities(FMassEntityManager& EntityManager,
                                         FMassExecutionContext& Context,
                                         FMassSignalNameLookup& EntitySignals)
{
    // 1) Gather *all* alive units into flat arrays so we can iterate
    //    over them per-detector.  (O(N) once per tick.)
    TArray<FMassEntityHandle> AllEntities;
    TArray<FTransform>        AllTransforms;
    TArray<FMassCombatStatsFragment>       AllStats;
    TArray<FMassAgentCharacteristicsFragment> AllChars;

    {
        FMassEntityQuery AllUnitsQuery;
        AllUnitsQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
        AllUnitsQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
        AllUnitsQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
        AllUnitsQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

        AllUnitsQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkCtx)
        {
            const int32 Num = ChunkCtx.GetNumEntities();
            const auto& TfView   = ChunkCtx.GetFragmentView<FTransformFragment>();
            const auto& StatsView= ChunkCtx.GetFragmentView<FMassCombatStatsFragment>();
            const auto& CharView = ChunkCtx.GetFragmentView<FMassAgentCharacteristicsFragment>();

            for (int32 i = 0; i < Num; ++i)
            {
                AllEntities.Add( ChunkCtx.GetEntity(i) );
                AllTransforms.Add( TfView[i].GetTransform() );
                AllStats.Add( StatsView[i] );
                AllChars.Add( CharView[i] );
            }
        });
    }

    if (AllEntities.Num() == 0)
    {
        // Nothing alive to detect
        return;
    }

    // 2) Now run your detector query, but *only* process those that actually got the ping
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
    [&](FMassExecutionContext& ChunkCtx)
    {
        const int32 Num = ChunkCtx.GetNumEntities();
        const auto& TfView       = ChunkCtx.GetFragmentView<FTransformFragment>();
        const auto&      TargetView    = ChunkCtx.GetMutableFragmentView<FMassAITargetFragment>();
        const auto& StatsView    = ChunkCtx.GetFragmentView<FMassCombatStatsFragment>();
        const auto& CharView     = ChunkCtx.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < Num; ++i)
        {
            const FMassEntityHandle Me = ChunkCtx.GetEntity(i);

            // did I get a UnitInDetectionRange signal?
            TArray<FName> MySignals;
            EntitySignals.GetSignalsForEntity(Me, MySignals);
            if (!MySignals.Contains(UnitSignals::UnitInDetectionRange))
            {
                // nope—skip
                continue;
            }

            // pull my data
            const FTransform&                   MeTf    = TfView[i].GetTransform();
            const FMassCombatStatsFragment&    MeStats = StatsView[i];
            const FMassAgentCharacteristicsFragment& MeChars = CharView[i];

            const FVector MeLoc           = MeTf.GetLocation();
            const float   SightSq         = FMath::Square(MeStats.SightRadius);
            const float   LoseSightSq     = FMath::Square(MeStats.LoseSightRadius);

            // best‑target tracking
            FMassEntityHandle BestTarget;
            float MinDistSq = SightSq;
            FVector BestLoc = FVector::ZeroVector;
            bool bFound   = false;
            bool bStillOK = false;
            FVector LastKnown = FVector::ZeroVector;

            // 3) N² search: check every other unit
            for (int32 j = 0; j < AllEntities.Num(); ++j)
            {
                const FMassEntityHandle They = AllEntities[j];
                if (They == Me) 
                {
                    continue;
                }

                // same‑team?
                if (AllStats[j].TeamId == MeStats.TeamId)
                {
                    continue;
                }

                // can I see/fight them?
                const auto& TChars = AllChars[j];
                if (TChars.bIsInvisible && !MeChars.bCanDetectInvisible) 
                {
                    continue;
                }
                const bool bCanAttack = ( TChars.bIsFlying   && MeChars.bCanAttackFlying ) ||
                                        (!TChars.bIsFlying && MeChars.bCanAttackGround );
                if (!bCanAttack) 
                {
                    continue;
                }

                // distance
                const FVector ThemLoc = AllTransforms[j].GetLocation();
                const float   Dsq     = FVector::DistSquared(MeLoc, ThemLoc);
                if (Dsq > SightSq) 
                {
                    continue;
                }

                // is this closer?
                if (Dsq < MinDistSq)
                {
                    MinDistSq      = Dsq;
                    BestTarget     = They;
                    BestLoc        = ThemLoc;
                    bFound         = true;
                }

                // did we already have a target, and is *that* one still in lose‑range?
                if (TargetView[i].TargetEntity.IsSet() && They == TargetView[i].TargetEntity)
                {
                    if (Dsq <= LoseSightSq)
                    {
                        bStillOK     = true;
                        LastKnown    = ThemLoc;
                    }
                }
            }

            // 4) Write back into the AITarget fragment + tag
            const bool bHadTarget = TargetView[i].bHasValidTarget && TargetView[i].TargetEntity.IsSet();
            if (bFound)
            {
                TargetView[i].TargetEntity      = BestTarget;
                TargetView[i].LastKnownLocation = BestLoc;
                TargetView[i].bHasValidTarget   = true;
            }
            else if (bHadTarget && bStillOK)
            {
                // keep old target
                TargetView[i].LastKnownLocation = LastKnown;
                TargetView[i].bHasValidTarget   = true;
            }
            else
            {
                // lost it
                TargetView[i].TargetEntity.Reset();
                TargetView[i].bHasValidTarget = false;
            }

            // tag add/remove
            if (TargetView[i].bHasValidTarget)
            {
                ChunkCtx.Defer().AddTag<FMassHasTargetTag>(Me);
            }
            else
            {
                ChunkCtx.Defer().RemoveTag<FMassHasTargetTag>(Me);
            }
        }
    });
}



/*
void UDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Einfaches Throttling (BESSER: Prozessor seltener ticken lassen!) ---
    //UE_LOG(LogTemp, Log, TEXT("Execute DetectionProcessor!!!!!!!!!"));
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < DetectionInterval)
    {
        return;
    }
    TimeSinceLastRun = 0.0f;
    // --- Ende Throttling ---
    //UE_LOG(LogTemp, Log, TEXT("DetectionProcessor!!!!!!!!!"));
    
    // HINWEIS ZUR PERFORMANCE:
    // Die folgende Implementierung ist ein PLATZHALTER und EXTREM INEFFIZIENT (N^2 Komplexität)!
    // Eine echte Implementierung MUSS Spatial Queries verwenden (Octree, MassSignalSubsystem etc.),
    // um nur Entitäten in der Nähe zu prüfen.

    // 1. Sammle relevante Daten aller potenziellen Ziele (SEHR LANGSAM!)
    TArray<FMassEntityHandle> AllEntities;
    TArray<FTransformFragment> AllTransforms; // Beachte: Dies kopiert die Fragmente! Ineffizient.
    TArray<FMassCombatStatsFragment> AllStats;
    TArray<FMassAgentCharacteristicsFragment> AllCharacteristics;

  
    FMassEntityQuery AllUnitsQuery;
    AllUnitsQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    AllUnitsQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    AllUnitsQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    AllUnitsQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None); // Schließe tote direkt aus
    // AllUnitsQuery.CacheFragmentViews(EntityManager); // <--- GELÖSCHT/FEHLERHAFT

    AllUnitsQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
        {
            const int32 Num = ChunkContext.GetNumEntities();
            const auto TransformsView = ChunkContext.GetFragmentView<FTransformFragment>();
            const auto StatsView = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
            const auto CharsView = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

            for (int32 i = 0; i < Num; ++i)
            {
                AllEntities.Add(ChunkContext.GetEntity(i));
                AllTransforms.Add(TransformsView[i]); // Kopiert das Fragment
                AllStats.Add(StatsView[i]);
                AllCharacteristics.Add(CharsView[i]);
            }
        });

    if (AllEntities.IsEmpty()) return; // Keine Ziele zum Prüfen

    // 2. Iteriere durch die Entitäten DIESES Prozessors und prüfe gegen alle anderen
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);
            // --- KORREKTUR 1 & 3 ---
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            FMassAITargetFragment& CurrentTarget = TargetList[i];
            const FMassCombatStatsFragment& CurrentStats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CurrentChars = CharList[i];

            FMassEntityHandle BestTargetEntity;
            float MinDistSq = FMath::Square(CurrentStats.SightRadius);
            FVector BestTargetLocation = FVector::ZeroVector;
            bool bFoundTargetThisRun = false;

            // --- Zielsuche (N^2 - SEHR LANGSAM!) ---
            for (int32 j = 0; j < AllEntities.Num(); ++j)
            {
                const FMassEntityHandle PotentialTargetEntity = AllEntities[j];
                if (CurrentEntity == PotentialTargetEntity) continue;

                const FMassCombatStatsFragment& TargetStats = AllStats[j];
                if (CurrentStats.TeamId == TargetStats.TeamId) continue; // Gleiches Team überspringen

                // --- KORREKTUR 1 & 3 ---
                const FTransform& TargetTransform = AllTransforms[j].GetTransform();
                const float DistSq = FVector::DistSquared(CurrentTransform.GetLocation(), TargetTransform.GetLocation());

                if (DistSq < MinDistSq)
                {
                    const FMassAgentCharacteristicsFragment& TargetChars = AllCharacteristics[j];
                    if (TargetChars.bIsInvisible && !CurrentChars.bCanDetectInvisible) continue;
                    if (CurrentChars.bCanAttackFlying && TargetChars.bIsFlying) continue;
                    if (CurrentChars.bCanAttackGround && !TargetChars.bIsFlying) continue;

                    // TODO: Sichtlinien-Check

                    MinDistSq = DistSq;
                    BestTargetEntity = PotentialTargetEntity;
                    BestTargetLocation = TargetTransform.GetLocation();
                    bFoundTargetThisRun = true;
                }
            } // Ende Schleife über alle potenziellen Ziele

            // --- Zielverlust-Check ---
            bool bTargetWasValid = CurrentTarget.bHasValidTarget; // Vorherigen Zustand merken
            if (CurrentTarget.bHasValidTarget && CurrentTarget.TargetEntity.IsSet())
            {
                bool bTargetStillValidAndInRange = false;
                for (int32 j = 0; j < AllEntities.Num(); ++j)
                {
                    if (AllEntities[j] == CurrentTarget.TargetEntity)
                    {
                        // --- KORREKTUR 1 & 3 ---
                        const FTransform& TargetTransform = AllTransforms[j].GetTransform();
                        const float DistSq = FVector::DistSquared(CurrentTransform.GetLocation(), TargetTransform.GetLocation());
                        if (DistSq <= FMath::Square(CurrentStats.LoseSightRadius))
                        {
                            // TODO: Sichtlinien-Check
                            bTargetStillValidAndInRange = true;
                            CurrentTarget.LastKnownLocation = TargetTransform.GetLocation();
                        }
                        break;
                    }
                }
                if (!bTargetStillValidAndInRange)
                {
                    // Ziel verloren
                    CurrentTarget.TargetEntity.Reset();
                    CurrentTarget.bHasValidTarget = false;
                }
            }

            // --- Bestes gefundenes Ziel setzen ---
            if (bFoundTargetThisRun) // Wenn wir in *diesem* Durchlauf ein Ziel gefunden haben
            {
                 // Setze es nur, wenn es besser ist oder wir kein gültiges hatten
                 if (!CurrentTarget.bHasValidTarget || BestTargetEntity != CurrentTarget.TargetEntity)
                 {
                    CurrentTarget.TargetEntity = BestTargetEntity;
                    CurrentTarget.LastKnownLocation = BestTargetLocation;
                    CurrentTarget.bHasValidTarget = true;
                 }
                 // Ansonsten behalte das alte Ziel, auch wenn es weiter weg ist als ein anderes potenzielles Ziel in Sichtweite
            }
             else if (bTargetWasValid && !CurrentTarget.bHasValidTarget)
             {
                 // Kein neues Ziel gefunden, aber das alte wurde gerade im Verlust-Check oben als ungültig markiert.
                 // Zustand ist schon korrekt (bHasValidTarget = false).
             }


            // --- Tag Management (KORREKTUR 2) ---
            // Füge/Entferne den Tag basierend auf dem *finalen* Zustand von bHasValidTarget
            if (CurrentTarget.bHasValidTarget)
            {
                ChunkContext.Defer().AddTag<FMassHasTargetTag>(CurrentEntity);
            }
            else
            {
                ChunkContext.Defer().RemoveTag<FMassHasTargetTag>(CurrentEntity);
            }

        } // Ende Schleife für Entities dieses Prozessors
    }); // Ende ForEachEntityChunk für diesen Prozessor
}*/