#include "Mass/States/PauseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform

UPauseStateProcessor::UPauseStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UPauseStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All); // Nur Pause-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer lesen/schreiben, Zustand ändern
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Stats lesen (AttackPauseDuration)
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Sicherstellen, dass Velocity 0 ist
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position für Distanzcheck

    EntityQuery.RegisterWithProcessor(*this);
}

void UPauseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
         const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassVelocityFragment& Velocity = VelocityList[i];

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1. Sicherstellen, dass Einheit steht
            Velocity.Value = FVector::ZeroVector;

             // 2. Ziel verloren oder ungültig? -> Zurück zu Idle (oder vorherigem Zustand)
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                ChunkContext.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStateIdleTag>(Entity); // Oder StateFrag.PreviousState Tag
                StateFrag.StateTimer = 0.f;
                continue;
            }

            // 3. Timer für Pause-Dauer prüfen
            StateFrag.StateTimer += DeltaTime;
            if (StateFrag.StateTimer >= Stats.AttackPauseDuration) // AttackPauseDuration muss im StatsFragment sein
            {
                // Pause vorbei, prüfe ob Angriff möglich ist
                const float EffectiveAttackRange = Stats.AttackRange + Stats.AgentRadius;
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                     // Noch/wieder in Reichweite -> Wechsle zu Attack
                     ChunkContext.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                     ChunkContext.Defer().AddTag<FMassStateAttackTag>(Entity);
                     StateFrag.StateTimer = 0.f; // Reset Timer für Attack-Dauer
                     // Hier könnte ein Signal gesendet werden "StartAttackAnimation"
                }
                else
                {
                     // Nicht mehr in Reichweite -> Wechsle zurück zu Chase
                     ChunkContext.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                     ChunkContext.Defer().AddTag<FMassStateChaseTag>(Entity);
                     StateFrag.StateTimer = 0.f;
                }
                continue; // Zustand gewechselt
            }

            // 4. In Pause bleiben, optional Rotation zum Ziel implementieren
            // (Besser in separatem LookAtProcessor)
        }
    });
}