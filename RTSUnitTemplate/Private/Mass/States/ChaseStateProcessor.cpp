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

    // Optional: FMassActorFragment für Rotation oder Fähigkeits-Checks?

    EntityQuery.RegisterWithProcessor(*this);
}

void UChaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor::Execute!")); // Log entry

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
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                 if (!SignalSubsystem)
                 {
                      continue; // Handle missing subsystem
                 }
                 SignalSubsystem->SignalEntity(
                 UnitSignals::Idle,
                 Entity);
                
                ChunkContext.Defer().RemoveTag<FMassStateChaseTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStateIdleTag>(Entity); // Oder StateFrag.PreviousState Tag
                
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
                
                ChunkContext.Defer().RemoveTag<FMassStateChaseTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStatePauseTag>(Entity); // Zu Pause, nicht direkt Attack

                StateFrag.StateTimer = 0.f; // Reset Timer für Pause/Attack
                continue;
            }
         
            // 4. Außer Reichweite -> Weiter verfolgen
            UpdateMoveTarget(MoveTarget, TargetFrag.LastKnownLocation, Stats.RunSpeed, World);
        }
    });
}

void UChaseStateProcessor::UpdateMoveTarget(FMassMoveTargetFragment& MoveTarget, const FVector& TargetLocation, float Speed, UWorld* World)
{
    // --- Log Entry Point and Inputs ---
    UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor::UpdateMoveTarget: ---- Entered Function ----")); // Log entry

    // Sicherheitscheck für World Pointer
    if (!World)
    {
        // Log the error and exit
        UE_LOG(LogTemp, Error, TEXT("UChaseStateProcessor::UpdateMoveTarget: World is null! Cannot update MoveTarget."));
        return;
    }

    // Log Input Values
    // Using %s for FVector::ToString() requires the '*' to get the TCHAR*
    // Using %.2f for float Speed to show two decimal places
    UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor::UpdateMoveTarget: Input - TargetLocation: %s, Speed: %.2f"), *TargetLocation.ToString(), Speed);

    // --- Modify the Fragment ---
    MoveTarget.CreateNewAction(EMassMovementAction::Move, *World); // Wichtig: Aktion neu erstellen!
    MoveTarget.Center = TargetLocation;
    MoveTarget.DesiredSpeed.Set(Speed);
    MoveTarget.IntentAtGoal = EMassMovementAction::Stand; // Anhalten, wenn Ziel erreicht (oder was immer gewünscht ist)
    MoveTarget.SlackRadius = 50.f; // Standard-Akzeptanzradius für Bewegung (ggf. anpassen)

    // --- Forward Vector Calculation ---
    // IMPORTANT: This calculation currently uses TargetLocation - MoveTarget.Center.
    // Since MoveTarget.Center was just set to TargetLocation, this difference will be ZERO.
    // GetSafeNormal() on a ZeroVector returns FVector::ZeroVector.
    // You likely want to calculate the Forward vector based on the *current* entity location,
    // which isn't passed to this function. Assuming the current logic is intended (e.g., maybe
    // the movement system interprets ZeroVector Forward differently), we'll log it as is.
    FVector PreNormalizedForward = (TargetLocation - MoveTarget.Center);
    MoveTarget.Forward = PreNormalizedForward.GetSafeNormal();

    // --- Log Final State of the Fragment ---
    // Log the values *after* modification to confirm they were set correctly.
    // Assuming FMassDesiredSpeed has a .Get() method or similar to retrieve the float value.
    // Logging Enum as integer for simplicity: (int32)MoveTarget.IntentAtGoal
    UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor::UpdateMoveTarget: Output - MoveTarget Updated - Center: %s, DesiredSpeed: %.2f, SlackRadius: %.1f, Forward: %s, IntentAtGoal: %d"),
           *MoveTarget.Center.ToString(),
           MoveTarget.DesiredSpeed.Get(), // Adjust if .Get() is not the correct way to access the speed float
           MoveTarget.SlackRadius,
           *MoveTarget.Forward.ToString(),
           (int32)MoveTarget.IntentAtGoal);

    // Log the potentially problematic PreNormalizedForward vector
     if (PreNormalizedForward.IsNearlyZero())
     {
         UE_LOG(LogTemp, Warning, TEXT("UChaseStateProcessor::UpdateMoveTarget: PreNormalizedForward vector was %s (nearly zero), resulting Forward is %s."), *PreNormalizedForward.ToString(), *MoveTarget.Forward.ToString());
     }

     UE_LOG(LogTemp, Log, TEXT("UChaseStateProcessor::UpdateMoveTarget: ---- Exiting Function ----")); // Log exit
}

void UChaseStateProcessor::StopMovement(FMassMoveTargetFragment& MoveTarget, UWorld* World)
{
    // Sicherheitscheck für World Pointer
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("StopMovement: World is null!"));
        return;
    }

    // Modifiziere das übergebene Fragment direkt
    MoveTarget.CreateNewAction(EMassMovementAction::Stand, *World); // Wichtig: Aktion neu erstellen!
    MoveTarget.DesiredSpeed.Set(0.f);
    // Andere Felder wie Center, SlackRadius etc. bleiben unverändert, sind aber für Stand egal.
}