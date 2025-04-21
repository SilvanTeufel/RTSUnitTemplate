#include "Mass/States/PatrolRandomStateProcessor.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "NavigationSystem.h" // Für GetRandomReachablePointInRadius

// Fragmente und Tags
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"

#include "MassActorSubsystem.h"   
#include "MassNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h" // Für Cast
#include "Actors/Waypoint.h"      // Für Waypoint-Interaktion (falls noch nötig)
#include "Mass/Signals/MySignals.h"

UPatrolRandomStateProcessor::UPatrolRandomStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UPatrolRandomStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadWrite); // Random Target setzen
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Aktuelle Position

    // Optional: ActorFragment für komplexere Abfragen (z.B. GetWorld, NavSys)
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);

    // Ignoriere Einheiten, die bereits am Ziel sind (vom Movement gesetzt)
    EntityQuery.AddTagRequirement<FMassReachedDestinationTag>(EMassFragmentPresence::None);


    EntityQuery.RegisterWithProcessor(*this);
}


void UPatrolRandomStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
     UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context.GetWorld());
     UWorld* World = Context.GetWorld();
     if(!NavSys || !World) return;


    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto PatrolList = ChunkContext.GetMutableFragmentView<FMassPatrolFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();
        	
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            FMassPatrolFragment& PatrolFrag = PatrolList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformFragments[i].GetTransform();
    

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

             // 1. Ziel gefunden? -> Zu Chase wechseln
             if (TargetFrag.bHasValidTarget)
             {
             	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
				if (!SignalSubsystem)
				{
					 continue; // Handle missing subsystem
				}
				SignalSubsystem->SignalEntity(
				UnitSignals::Chase,
				Entity);
             	
             	StateFrag.StateTimer = 0.f;
             	
             	continue;
             }

             // 2. Ist aktuelles Bewegungsziel noch gültig oder muss ein neues gesetzt werden?
             //    (z.B. wenn MoveTarget leer ist oder Distanz zum Ziel sehr klein ist)
             const float DistToCurrentTargetSq = FVector::DistSquared(Transform.GetLocation(), MoveTarget.Center);
             const bool bHasReachedCurrentTarget = DistToCurrentTargetSq <= FMath::Square(MoveTarget.SlackRadius);

             if (bHasReachedCurrentTarget || MoveTarget.GetCurrentAction() != EMassMovementAction::Move)
             {

             	float IdleChance = 30.0f; // Beispiel: 30% Chance zu idlen
                  if (FMath::FRand() * 100.0f < IdleChance)
                  {
                  	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
					if (!SignalSubsystem)
					{
						 continue; // Handle missing subsystem
					}
					SignalSubsystem->SignalEntity(
					UnitSignals::PatrolIdle,
					Entity);
                  	
                  	StateFrag.StateTimer = 0.f; // Timer für Idle-Dauer starten
                  	continue;
                  }
                  else
                  {
                       // 3b. Kein Idle -> Neues zufälliges Ziel setzen
                       SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, NavSys, World);
                       // MoveTarget wird in der Helper-Funktion aktualisiert
                        if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move) {
                             StateFrag.StateTimer = 0.f; // Reset Timer bei neuem Ziel
                        } else {
                             // Konnte kein neues Ziel finden -> vielleicht zu Idle wechseln?
                        	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							   if (!SignalSubsystem)
							   {
									continue; // Handle missing subsystem
							   }
							   SignalSubsystem->SignalEntity(
							   UnitSignals::Idle,
							   Entity);
                        	
                             StateFrag.StateTimer = 0.f;
                             continue;
                        }
                  }
             }
             // 4. Ansonsten: Aktuelles Bewegungsziel beibehalten (wird von MovementProcessor verfolgt)
        }
    });
}

void UPatrolRandomStateProcessor::SetNewRandomPatrolTarget(FMassPatrolFragment& PatrolFrag, FMassMoveTargetFragment& MoveTarget, UNavigationSystemV1* NavSys, UWorld* World)
{
    // Diese Funktion repliziert die Logik aus SetPatrolCloseLocation / SetUEPathfindingRandomLocation
    // Braucht Zugriff auf Waypoint-Daten (Annahme: in PatrolFrag oder über Actor/Entity)

    // --- Beispiel: Hole "Basis"-Wegpunkt-Position ---
    // Dies ist der schwierigste Teil ohne die genaue Waypoint-Struktur zu kennen.
    // Annahme: Wir haben die Position des "Haupt"-Wegpunkts in PatrolFrag.TargetWaypointLocation
    FVector BaseWaypointLocation = PatrolFrag.TargetWaypointLocation; // Muss korrekt gesetzt sein!
    if (BaseWaypointLocation == FVector::ZeroVector)
    {
         // Fallback oder Fehlerbehandlung, wenn keine Basisposition bekannt ist
         MoveTarget.CreateNewAction(EMassMovementAction::Stand, *World); // Anhalten
         MoveTarget.DesiredSpeed.Set(0.f);
         UE_LOG(LogTemp, Warning, TEXT("SetNewRandomPatrolTarget: BaseWaypointLocation is Zero!"));
         return;
    }

    FNavLocation RandomPoint;
    bool bSuccess = false;
    int Attempts = 0;
    constexpr int MaxAttempts = 5;

    while (!bSuccess && Attempts < MaxAttempts)
    {
         // Finde zufälligen Punkt im Radius um den Basis-Wegpunkt
         bSuccess = NavSys->GetRandomReachablePointInRadius(BaseWaypointLocation, PatrolFrag.RandomPatrolRadius, RandomPoint);
         Attempts++;
    }


    if (bSuccess)
    {
        // UE_LOG(LogTemp, Log, TEXT("SetNewRandomPatrolTarget: New Target %s"), *RandomPoint.Location.ToString());
        MoveTarget.CreateNewAction(EMassMovementAction::Move, *World);
        MoveTarget.Center = RandomPoint.Location;
        // Geschwindigkeit aus Stats holen (benötigt Stats Fragment hier)
        // TODO: Hier Zugriff auf FMassCombatStatsFragment bekommen, wenn nötig!
        MoveTarget.DesiredSpeed.Set(600.f); // Beispiel: Feste Geschwindigkeit hier
        MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
        MoveTarget.SlackRadius = 50.f; // Standard-Akzeptanzradius
        PatrolFrag.TargetWaypointLocation = RandomPoint.Location; // Speichere das neue Ziel
    }
    else
    {
         // Konnte keinen Punkt finden, anhalten
         UE_LOG(LogTemp, Warning, TEXT("SetNewRandomPatrolTarget: Failed to find reachable point near %s after %d attempts."), *BaseWaypointLocation.ToString(), MaxAttempts);
         MoveTarget.CreateNewAction(EMassMovementAction::Stand, *World);
         MoveTarget.DesiredSpeed.Set(0.f);
    }

    // Die alte Logik mit LineTrace zum Boden anpassen, falls nötig (NavSys macht das oft schon)
    // FHitResult Hit;
    // FVector Start = RandomPoint.Location + FVector(0,0,1000);
    // FVector End = RandomPoint.Location - FVector(0,0,1000);
    // if(World->LineTrace...) RandomPoint.Location.Z = Hit.ImpactPoint.Z;
}