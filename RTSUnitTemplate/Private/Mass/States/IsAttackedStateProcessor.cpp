// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/IsAttackedStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"


UIsAttackedStateProcessor::UIsAttackedStateProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UIsAttackedStateProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::All); // Nur für Entities in diesem Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Dauer lesen

	// Optionale, aber oft nützliche Fragmente (nicht unbedingt für DIESE Logik nötig)
	// EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	// EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);

	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void UIsAttackedStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UIsAttackedStateProcessor_Execute); // Performance-Messung (optional)
    // UE_LOG(LogTemp, Log, TEXT("UIsAttackedStateProcessor::Execute"));

    UWorld* World = GetWorld(); // Welt holen (ist in UObject verfügbar)
    if (!World) return;

    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [this, SignalSubsystem](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];

            // Timer hochzählen
            StateFrag.StateTimer += DeltaTime;

            // Prüfen, ob die Dauer überschritten wurde
            // Annahme: IsAttackedDuration ist ein Member von FMassCombatStatsFragment
            if (StateFrag.StateTimer > StatsFrag.IsAttackedDuration)
            {
                //UE_LOG(LogTemp, Log, TEXT("Entity %d:%d IsAttacked duration ended. Signaling Pause."), Entity.Index, Entity.SerialNumber);

                // Zum Pause-Zustand wechseln
                SignalSubsystem->SignalEntity(UnitSignals::Run, Entity);

                // Timer für den nächsten Zustand (Pause) zurücksetzen
                // Der PauseStateProcessor wird diesen Timer dann verwenden.
                StateFrag.StateTimer = 0.f;
                // Da der Zustand gewechselt wird, brauchen wir hier nichts weiter zu tun
                // Die Tags werden vom UnitStateProcessor (Signal Handler) geändert.
            }
            // Ansonsten: Bleibe im IsAttacked Zustand. Keine weitere Aktion nötig.
        }
    });
}
