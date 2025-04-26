// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/CastingStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"


UCastingStateProcessor::UCastingStateProcessor()
{
	// Standard-Einstellungen, können angepasst werden
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UCastingStateProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::All); // Nur für Entities in diesem Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);     // Timer lesen/schreiben
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);   // CastTime lesen
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);     // Geschwindigkeit auf 0 setzen

	// Optionale Fragmente (falls benötigt, z.B. für Actor-Interaktion oder komplexere Logik)
	// EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	// EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);

	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
	
	EntityQuery.RegisterWithProcessor(*this);
}

void UCastingStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
  QUICK_SCOPE_CYCLE_COUNTER(STAT_UCastingStateProcessor_Execute);
    // UE_LOG(LogTemp, Log, TEXT("UCastingStateProcessor::Execute"));

    UWorld* World = GetWorld();
    if (!World) return;

    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, SignalSubsystem](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];

            // 1. Bewegung stoppen
            //Velocity.Value = FVector::ZeroVector;

            // 2. Rotation wird vom ULookAtProcessor gehandhabt (falls für Casting konfiguriert)

            // 3. Timer hochzählen
            StateFrag.StateTimer += DeltaTime;

            // 4. Prüfen, ob Cast-Zeit abgelaufen ist
            // Annahme: CastTime ist ein Member von FMassCombatStatsFragment
            if (StateFrag.StateTimer > StatsFrag.CastTime)
            {
                // UE_LOG(LogTemp, Log, TEXT("Entity %d:%d Cast finished. Signaling CastComplete and Idle."), Entity.Index, Entity.SerialNumber);
                // In einen Standardzustand zurückwechseln (z.B. Idle)
                // Alternativ: Basierend auf Target Anwesenheit zu Chase wechseln?
                SignalSubsystem->SignalEntity(UnitSignals::Run, Entity);
                // Zustand wird vom UnitStateProcessor (Signal Handler) geändert.
            }
            // Ansonsten: Bleibe im Casting Zustand.
        }
    });
}
