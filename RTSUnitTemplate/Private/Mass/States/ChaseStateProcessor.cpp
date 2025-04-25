#include "Mass/States/ChaseStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationTypes.h"

#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"

#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"


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

    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

    EntityQuery.RegisterWithProcessor(*this);
}

void UChaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor::Execute!")); // Log entry

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

        //UE_LOG(LogTemp, Log, TEXT("Chase EntityCount:! %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            const FMassCombatStatsFragment& Stats = StatsList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
         
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            //UE::Mass::Debug::LogEntityTags(Entity, EntityManager, this);
            // 1. Ziel verloren oder ungültig? -> Zurück zu Idle (oder vorherigem Zustand)
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                 if (!SignalSubsystem)
                 {
                      continue; // Handle missing subsystem
                 }
                 SignalSubsystem->SignalEntity(
                 UnitSignals::Run,
                 Entity);
                StateFrag.StateTimer = 0.f; // Reset Timer
                continue;
            }

            // 2. Distanz zum Ziel prüfen
            // Annahme: AgentRadius enthält relevante Größe des Ziels und der eigenen Einheit
            const float EffectiveAttackRange = Stats.AttackRange; // + Stats.AgentRadius; // Vereinfacht
            const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
            const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

            // 3. In Angriffsreichweite?
            if (DistSq <= AttackRangeSq)
            {
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem)
                {
                     continue; // Handle missing subsystem
                }
                SignalSubsystem->SignalEntity(
                UnitSignals::Pause,
                Entity);

                StopMovement(MoveTarget, World);
                // UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor:: SET TO PAUSE!!!!!!!!!!!!!!!!!!!!!!!")); 
                StateFrag.StateTimer = 0.f; // Reset Timer für Pause/Attack
                continue;
            }
         
            // 4. Außer Reichweite -> Weiter verfolgen
            UpdateMoveTarget(MoveTarget, TargetFrag.LastKnownLocation, Stats.RunSpeed, World);
        }
    });
}