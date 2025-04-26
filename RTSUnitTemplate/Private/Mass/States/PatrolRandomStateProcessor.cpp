#include "Mass/States/PatrolRandomStateProcessor.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

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
	EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::All); // Nur Chase-Entitäten

	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Zustand ändern, Timer lesen
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly); // Ziel lesen
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly); // Eigene Position lesen
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly); // Eigene Stats lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Bewegungsziel setzen
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Geschwindigkeit setzen (zum Stoppen)

	// ***** ADD THIS LINE *****
	EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadWrite); // Request the patrol fragment
	// ***** END ADDED LINE *****
	
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    // Optional: ActorFragment für komplexere Abfragen (z.B. GetWorld, NavSys)
   // EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);

    // Ignoriere Einheiten, die bereits am Ziel sind (vom Movement gesetzt)
    //EntityQuery.AddTagRequirement<FMassReachedDestinationTag>(EMassFragmentPresence::None);


    EntityQuery.RegisterWithProcessor(*this);
}


void UPatrolRandomStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("UPatrolRandomStateProcessor!!!!!"));
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

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        	
        for (int32 i = 0; i < NumEntities; ++i)
        {

        	UE_LOG(LogTemp, Log, TEXT("PatrolRandom EntityCount:! %d"), NumEntities);
        	
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            FMassPatrolFragment& PatrolFrag = PatrolList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformFragments[i].GetTransform();
    
        	StateFrag.StateTimer += DeltaTime;
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

        	/*
			if (StateFrag.StateTimer <= DeltaTime)
			{
				UE_LOG(LogTemp, Log, TEXT("SetNewRandomPatrolTarget!!!!!!!!"));
				SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, NavSys, World, Stats.RunSpeed);
			}*/

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
             	continue;
             }
        	
             // 2. Ist aktuelles Bewegungsziel noch gültig oder muss ein neues gesetzt werden?
             //    (z.B. wenn MoveTarget leer ist oder Distanz zum Ziel sehr klein ist)
        	/*
             const float DistToCurrentTargetSq = FVector::DistSquared(Transform.GetLocation(), MoveTarget.Center);
             const bool bHasReachedCurrentTarget = DistToCurrentTargetSq <= FMath::Square(MoveTarget.SlackRadius);

             if (bHasReachedCurrentTarget || MoveTarget.GetCurrentAction() != EMassMovementAction::Move)
             {
             	
                  if (FMath::FRand() * 100.0f < PatrolFrag.IdleChance)
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
             }*/
             // 4. Ansonsten: Aktuelles Bewegungsziel beibehalten (wird von MovementProcessor verfolgt)
        }
    });
}