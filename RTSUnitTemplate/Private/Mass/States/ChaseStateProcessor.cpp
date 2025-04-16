#include "Mass/States/ChaseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"


UChaseStateProcessor::UChaseStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UChaseStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::All); // Nur Chase-Entitäten

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)

    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

    EntityQuery.RegisterWithProcessor(*this);
}

void UChaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld(); // World für MoveTarget holen

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& Stats = StatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1. Ziel verloren oder ungültig? -> Zurück zu Idle (oder vorherigem Zustand)
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                ChunkContext.Defer().RemoveTag<FMassStateChaseTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStateIdleTag>(Entity); // Oder StateFrag.PreviousState Tag
                StopMovement(MoveTarget, World);
                VelocityList[i].Value = FVector::ZeroVector;
                StateFrag.StateTimer = 0.f; // Reset Timer
                continue;
            }

            // 2. Distanz zum Ziel prüfen
            // Annahme: AgentRadius enthält relevante Größe des Ziels und der eigenen Einheit
            const float EffectiveAttackRange = Stats.AttackRange + Stats.AgentRadius; // Vereinfacht
            const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
            const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

            // 3. In Angriffsreichweite?
            if (DistSq <= AttackRangeSq)
            {
                // Ja -> Anhalten und zu Pause wechseln (bereitet Angriff vor)
                ChunkContext.Defer().RemoveTag<FMassStateChaseTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStatePauseTag>(Entity); // Zu Pause, nicht direkt Attack
                StopMovement(MoveTarget, World);
                 VelocityList[i].Value = FVector::ZeroVector;
                StateFrag.StateTimer = 0.f; // Reset Timer für Pause/Attack
                continue;
            }
            else
            {
                // 4. Außer Reichweite -> Weiter verfolgen
                UpdateMoveTarget(MoveTarget, TargetFrag.LastKnownLocation, Stats.RunSpeed, World);

                // TODO: Hier könnte Evasion-Logik geprüft werden (z.B. auf Tag reagieren)
                // if (ChunkContext.DoesEntityHaveTag<FCollidingWithFriendlyTag>(Entity)) { ... -> Evasion }

                 // TODO: Hier könnten Fähigkeiten aktiviert werden (Offensive, Throw etc.)
                 // z.B. via Signal oder ActorFragment Call, wenn noch nicht im Cooldown
            }

            // Rotation wird idealerweise von UUnitMovementProcessor oder ULookAtProcessor gehandhabt
        }
    });
}

void UChaseStateProcessor::UpdateMoveTarget(FMassMoveTargetFragment& MoveTarget, const FVector& TargetLocation, float Speed, UWorld* World)
{
    /*
    MoveTarget.CreateNewAction(EMassMovementAction::Move, *World);
    MoveTarget.Center = TargetLocation;
    MoveTarget.DesiredSpeed.Set(Speed);
    MoveTarget.IntentAtGoal = EMassMovementAction::Stand; // Anhalten, wenn Ziel erreicht
    MoveTarget.SlackRadius = 50.f; // Standard-Akzeptanzradius für Bewegung

    */
}
 void UChaseStateProcessor::StopMovement(FMassMoveTargetFragment& MoveTarget, UWorld* World)
{
    /*
    MoveTarget.CreateNewAction(EMassMovementAction::Stand, *World);
    MoveTarget.DesiredSpeed.Set(0.f);
    */
}