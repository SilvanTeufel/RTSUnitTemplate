// In IdleStateProcessor.cpp

#include "Mass/States/IdleStateProcessor.h" // Dein Prozessor-Header

// Andere notwendige Includes...
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"      // FMassVelocityFragment
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

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
    UE_LOG(LogTemp, Log, TEXT("UIdleStateProcessor::Execute!"));
    const UWorld* World = EntityManager.GetWorld();
    if (!World) return;
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World](FMassExecutionContext& ChunkContext)
    {
        
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
 
            FMassAIStateFragment& StateFrag = StateList[i];

            // --- 3. Prüfen auf gefundenes Ziel ---
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            bool bCanAttack = true; // StatsFrag.bCanAttack; // Dein Flag hier

            if (TargetFrag.bHasValidTarget && bCanAttack /* && Bedingungen */)
            {
                UE_LOG(LogTemp, Log, TEXT("IDLE TO CHASE!!!!!!!"));
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                  if (!SignalSubsystem)
                  {
                       continue; // Handle missing subsystem
                  }
                  SignalSubsystem->SignalEntity(
                  UnitSignals::Chase,
                  Entity);
                
                StateFrag.StateTimer = 0.f; // Timer über State Fragment zurücksetzen
                continue;
            }
            
            StateFrag.StateTimer += DeltaTime;
            const FMassPatrolFragment& PatrolFrag = PatrolList[i];
            bool bHasPatrolRoute = PatrolFrag.CurrentWaypointIndex != INDEX_NONE;
            bool bIsOnPlattform = false; // Dein Flag hier aus Stats o.ä.

            if (!bIsOnPlattform && bSetUnitsBackToPatrol && bHasPatrolRoute && StateFrag.StateTimer >= SetUnitsBackToPatrolTime)
            {
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                  if (!SignalSubsystem)
                  {
                       continue; // Handle missing subsystem
                  }
                  SignalSubsystem->SignalEntity(
                  UnitSignals::PatrolRandom,
                  Entity);
                
                 // === KORREKTUR HIER ===
                StateFrag.StateTimer = 0.f; // Timer über State Fragment zurücksetzen
                continue;
            }

            // --- 5. Im Idle-Zustand bleiben ---

        } // End for each entity
    }); // End ForEachEntityChunk
}