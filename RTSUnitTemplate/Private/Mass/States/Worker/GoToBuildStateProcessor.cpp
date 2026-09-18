// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/Worker/GoToBuildStateProcessor.h" // Adjust path
#include "Actors/WorkArea.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Unit/WorkingUnitBase.h"

// Engine & Mass includes
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"     // For FMassSignalPayload and sending signals
#include "Async/Async.h"
#include "Engine/World.h"

// Your project specific includes
#include "MassNavigationFragments.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"
#include "ProfilingDebugging/CsvProfiler.h"


UGoToBuildStateProcessor::UGoToBuildStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UGoToBuildStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // NO FMassActorFragment
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadOnly); // Contains target pos/radius now
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);       // For speed
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // Client prediction: filled on the client to anticipate the server stop (Optional so the
    // server query still matches entities that lack it). Actor is read for MovementAcceptanceRadius.
    EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::All);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBuildStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGoToBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UGoToBuildStateProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UGoToBuildStateProcessor);

    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun -= ExecutionInterval;

    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

// CLIENT: prediction only. Never authors state tags (the bubble is the sole tag authority for
// workers) and never writes the authoritative MoveTarget. WorkerStats is NOT replicated, so the
// destination comes from the replicated MoveTarget.Center and the arrival distance is recomputed
// from the replicated MovementAcceptanceRadius.
void UGoToBuildStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetMutableFragmentView<FMassClientPredictionFragment>();
        if (PredList.Num() == 0) return; // no prediction fragment on this archetype -> nothing to do

        const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FVector CurrentLocation = TransformList[i].GetTransform().GetLocation();
            const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            float ArrivalDistance = ArrivalDistanceMultiplier * 50.f; // fallback if actor not resolved yet
            if (ActorList.Num() > 0)
            {
                if (const AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].Get()))
                {
                    ArrivalDistance = ArrivalDistanceMultiplier * Unit->MovementAcceptanceRadius;
                }
            }

            PredictWorkerStop(PredList[i], CurrentLocation, MoveTarget.Center, MoveTarget.DesiredSpeed.Get(), ArrivalDistance);
        }
    });
}

void UGoToBuildStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();

    if (!World)
    {
        return;
    }

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, World](FMassExecutionContext& Context)
    {
        // --- Get Fragment Views ---
        const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
        const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
        const TConstArrayView<FMassWorkerStatsFragment> WorkerStatsList = Context.GetFragmentView<FMassWorkerStatsFragment>();
        const TArrayView<FMassAIStateFragment> AIStateList = Context.GetMutableFragmentView<FMassAIStateFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = Context.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = Context.GetFragmentView<FMassAgentCharacteristicsFragment>();
        // Optional: nur fuer die Groesse dessen, was auf der Baustelle steht (siehe unten).
        const TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
 
        const int32 NumEntities = Context.GetNumEntities();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = Context.GetEntity(i);
            const FTransform& CurrentTransform = TransformList[i].GetTransform();
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassWorkerStatsFragment& WorkerStats = WorkerStatsList[i];
            FMassAIStateFragment& AIState = AIStateList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FMassAgentCharacteristicsFragment& CharFrag = CharList[i];
            // Increment state timer
            AIState.StateTimer += ExecutionInterval;
            
            // Basic validation of data from fragment (more robust checks should be external)
            if ((WorkerStats.BuildingAvailable || !WorkerStats.BuildingAreaAvailable) && !AIState.SwitchingState) // Example basic check
            {
                 if (SignalSubsystem)
                 {
                     SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::GoToBase, Entity);
                 }
                 continue;
            }
            
            const float DistanceToTargetCenter = FVector::Dist2D(CurrentTransform.GetLocation(), WorkerStats.BuildAreaPosition);

            // Groesse dessen, was auf der Baustelle steht, mitrechnen - wie es das Reparieren laengst
            // tut (FollowRadius + beide Kapselradien). Beim Bauen galt bisher ein FESTER Abstand zur
            // Mitte (5 x MovementAcceptanceRadius = 250 bei Vorgabe). Die ConstructionUnit wird aber
            // auf die Grundflaeche skaliert; ihre Kapsel erreicht dabei 350 und mehr. Ist sie groesser
            // als der Ankunftsabstand, kann der Arbeiter die Bedingung NIE erfuellen - er steht davor
            // und wartet. Deshalb kommt ihr Radius jetzt oben drauf.
            float Ankunftsabstand = WorkerStats.BuildAreaArrivalDistance;
            if (!ActorList.IsEmpty())
            {
                if (const AWorkingUnitBase* Arbeiter = Cast<AWorkingUnitBase>(ActorList[i].Get()))
                {
                    if (const AWorkArea* Flaeche = Arbeiter->BuildArea)
                    {
                        // Bezug ist die GRUNDFLAECHE der Baustelle, nicht die ConstructionUnit.
                        //
                        // Erster Versuch hing am Zeiger Flaeche->ConstructionUnit - der ist beim
                        // wartenden Arbeiter oft null (gemessen: CUDa=0 in praktisch allen Zeilen),
                        // dann blieb der Aufschlag aus und die Schwelle stand weiter bei 125, waehrend
                        // das Hindernis auf dem Platz mehrere hundert Einheiten misst. Die Flaeche
                        // selbst ist dagegen immer da, und die ConstructionUnit wird ohnehin auf genau
                        // diese Grundflaeche skaliert - sie ist also das richtige Mass.
                        float Belegt = 0.f;
                        if (const UStaticMeshComponent* FlaechenMesh = Flaeche->Mesh)
                        {
                            const FVector Ausdehnung = FlaechenMesh->Bounds.BoxExtent;
                            Belegt = FMath::Max(Ausdehnung.X, Ausdehnung.Y);
                        }
                        // Falls die ConstructionUnit doch greifbar ist und groesser misst, gewinnt sie.
                        if (const AUnitBase* Belegend = Flaeche->ConstructionUnit)
                        {
                            if (const UCapsuleComponent* Kapsel = Belegend->GetCapsuleComponent())
                            {
                                Belegt = FMath::Max(Belegt, Kapsel->GetScaledCapsuleRadius());
                            }
                        }
                        Ankunftsabstand += Belegt * Flaeche->ConstructionUnitReachFactor;
                    }
                }
            }

            // Marschbefehl nachfassen, wenn der Arbeiter unterwegs stehenbleibt.
            //
            // Gemessen am 19.08.: Arbeiter standen mit Abstand 4005 zur Baustelle und SollTempo 0 im
            // Zustand GoToBuild - dauerhaft. Dieser Prozessor erteilt naemlich SELBST keinen Laufbefehl;
            // er prueft nur Ankunft und Abbruch. Der Befehl kommt einmalig beim Zustandswechsel aus
            // UpdateUnitMovement. Faellt das Tempo danach auf 0 (Pfad zu Ende, Ziel verschoben,
            // Ankunft am alten Zielpunkt), steht der Arbeiter fuer immer und niemand schickt ihn los.
            //
            // Nur nachfassen, wenn er wirklich steht ODER das Ziel deutlich abweicht - jeden Takt neu
            // zu befehlen wuerde die Pfadsuche staendig neu starten und ihn erst recht anhalten
            // (derselbe Fehler wie beim Verfolgen bewegter Ziele).
            if (DistanceToTargetCenter > Ankunftsabstand && !AIState.SwitchingState)
            {
                const bool bStehtStill = MoveTarget.DesiredSpeed.Get() <= KINDA_SMALL_NUMBER;
                const bool bZielWeit = FVector::Dist2D(MoveTarget.Center, WorkerStats.BuildAreaPosition) > 250.f;
                if (bStehtStill || bZielWeit)
                {
                    UpdateMoveTarget(MoveTarget, WorkerStats.BuildAreaPosition, Stats.RunSpeed, World);
                }
            }

            MoveTarget.DistanceToGoal = DistanceToTargetCenter - Ankunftsabstand; // Update distance
            if (DistanceToTargetCenter <= Ankunftsabstand && !AIState.SwitchingState)
            {
                AIState.SwitchingState = true;
                // Stop movement immediately and mirror to all clients
                StopMovement(MoveTarget, World);
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::Build, Entity);
                }
                continue;
            }
        } // End loop through entities
    }); // End ForEachEntityChunk

}
