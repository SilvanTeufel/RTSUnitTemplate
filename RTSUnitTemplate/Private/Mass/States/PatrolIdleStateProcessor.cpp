#include "Mass/States/PatrolIdleStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"

UPatrolIdleStateProcessor::UPatrolIdleStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UPatrolIdleStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly); // Idle-Zeiten lesen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Velocity auf 0

    EntityQuery.RegisterWithProcessor(*this);
}

void UPatrolIdleStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
            TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>(); 
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        const float CurrentWorldTime = Context.GetWorld()->GetTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassPatrolFragment& PatrolFrag = PatrolList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1. Bewegung stoppen
            Velocity.Value = FVector::ZeroVector;

            // 2. Ziel gefunden? -> Zu Chase wechseln
             if (TargetFrag.bHasValidTarget)
             {
                 ChunkContext.Defer().RemoveTag<FMassStatePatrolIdleTag>(Entity);
                 ChunkContext.Defer().AddTag<FMassStateChaseTag>(Entity);
                 AUnitBase* Actor = Cast<AUnitBase>(ActorFragments[i].GetMutable());
                 Actor->SetUnitState(UnitData::Chase);
                 StateFrag.StateTimer = 0.f;
                 IdleEndTimes.Remove(Entity); // Eintrag entfernen
                 continue;
             }

             // 3. Idle-Timer prüfen
             if (StateFrag.StateTimer == 0.f) // Erster Tick in diesem Zustand
             {
                 // Berechne zufällige Idle-Dauer und speichere Endzeit
                 float IdleDuration = FMath::FRandRange(PatrolFrag.RandomPatrolMinIdleTime, PatrolFrag.RandomPatrolMaxIdleTime);
                 IdleEndTimes.FindOrAdd(Entity) = CurrentWorldTime + IdleDuration;
             }

             StateFrag.StateTimer += DeltaTime; // Timer hochzählen (obwohl wir Endzeit prüfen)

             // Finde die gespeicherte Endzeit
             float* EndTimePtr = IdleEndTimes.Find(Entity);
             if (EndTimePtr && CurrentWorldTime >= *EndTimePtr)
             {
                 // Idle-Zeit abgelaufen -> Wechsle zurück zu PatrolRandom
                 ChunkContext.Defer().RemoveTag<FMassStatePatrolIdleTag>(Entity);
                 ChunkContext.Defer().AddTag<FMassStatePatrolRandomTag>(Entity);
                 AUnitBase* Actor = Cast<AUnitBase>(ActorFragments[i].GetMutable());
                 Actor->SetUnitState(UnitData::PatrolRandom);
                 StateFrag.StateTimer = 0.f;
                 IdleEndTimes.Remove(Entity); // Eintrag entfernen
                 continue;
             }

             // 4. Sonst: In PatrolIdle bleiben
        }
    });

    // Optional: Aufräumen der Map für Entitäten, die nicht mehr existieren (seltener nötig)
    // TArray<FMassEntityHandle> StaleEntities;
    // for(auto const& [Entity, Time] : IdleEndTimes) { if(!EntityManager.IsEntityValid(Entity)) StaleEntities.Add(Entity); }
    // for(const auto& Stale : StaleEntities) { IdleEndTimes.Remove(Stale); }
}