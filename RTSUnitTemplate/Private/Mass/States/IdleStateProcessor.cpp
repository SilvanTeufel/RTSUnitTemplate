// In IdleStateProcessor.cpp

#include "Mass/States/IdleStateProcessor.h" // Dein Prozessor-Header

// Andere notwendige Includes...
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"      // FMassVelocityFragment
#include "Mass/UnitMassTag.h"

// ...

UIdleStateProcessor::UIdleStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UIdleStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);

    // Benötigte Fragmente:
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly);
    // === KORREKTUR HIER ===
    // Timer ist jetzt Teil von FMassAIStateFragment
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // ReadWrite für State und Timer

    EntityQuery.RegisterWithProcessor(*this);
}

void UIdleStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        // === KORREKTUR HIER ===
        // Mutable View für State Fragment holen (enthält den Timer)
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            // === KORREKTUR HIER ===
            // Referenz auf das State Fragment holen
            FMassAIStateFragment& StateFrag = StateList[i];

            // --- 1. Bewegung stoppen ---
            VelocityList[i].Value = FVector::ZeroVector;

            // --- (Platzhalter für Kollisionslogik) ---

            // --- 3. Prüfen auf gefundenes Ziel ---
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            bool bCanAttack = true; // StatsFrag.bCanAttack; // Dein Flag hier

            if (TargetFrag.bHasValidTarget && bCanAttack /* && Bedingungen */)
            {
                ChunkContext.Defer().RemoveTag<FMassStateIdleTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStateChaseTag>(Entity);
                // === KORREKTUR HIER ===
                StateFrag.StateTimer = 0.f; // Timer über State Fragment zurücksetzen
                continue;
            }

            // --- 4. Prüfen auf Rückkehr zur Patrouille ---
            // === KORREKTUR HIER ===
            // Timer über State Fragment inkrementieren und lesen
            StateFrag.StateTimer += DeltaTime;

            const FMassPatrolFragment& PatrolFrag = PatrolList[i];
            bool bHasPatrolRoute = PatrolFrag.CurrentWaypointIndex != INDEX_NONE;
            bool bIsOnPlattform = false; // Dein Flag hier aus Stats o.ä.

            if (!bIsOnPlattform && bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= SetUnitsBackToPatrolTime)
            {
                ChunkContext.Defer().RemoveTag<FMassStateIdleTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStatePatrolTag>(Entity);
                 // === KORREKTUR HIER ===
                StateFrag.StateTimer = 0.f; // Timer über State Fragment zurücksetzen
                continue;
            }

            // --- 5. Im Idle-Zustand bleiben ---

        } // End for each entity
    }); // End ForEachEntityChunk
}