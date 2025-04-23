#include "Mass/States/DeathStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"  
#include "MassCommonFragments.h" // Für FMassRepresentationFragment (optional für Sichtbarkeit)

// Für Actor Cast und Destroy
#include "MassSignalSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"


UDeathStateProcessor::UDeathStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior; // Oder eigene Gruppe
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UDeathStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::All); // Nur tote Entitäten
    
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Anhalten

    // Optional: Fragment, um zu sehen, ob Effekte schon abgespielt wurden
    // EntityQuery.AddRequirement<FDeathEffectStateFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.RegisterWithProcessor(*this);
}

void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld(); 
    UE_LOG(LogTemp, Log, TEXT("UDeathStateProcessor::Execute!"));
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];
            
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1. Bewegung stoppen
            Velocity.Value = FVector::ZeroVector;

            // 2. Timer hochzählen
            StateFrag.StateTimer += DeltaTime;

            // 3. Effekte beim ersten Eintritt in Dead State auslösen (Beispiel)
            if (StateFrag.StateTimer <= DeltaTime) // Nur im ersten Frame dieses Zustands
            {
                UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!SIGNEL TO START DEATH!!!!!!!!"));
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                 if (!SignalSubsystem)
                 {
                      continue; // Handle missing subsystem
                 }
                 SignalSubsystem->SignalEntity(
                 UnitSignals::StartDead,
                 Entity);
            }


            // 4. Despawn / Destroy nach Zeit
            if (StateFrag.StateTimer >= DespawnTime)
            {
              // Nur Entity zerstören, wenn kein Actor (mehr) da ist
              ChunkContext.Defer().DestroyEntity(Entity);
              // UE_LOG(LogTemp, Log, TEXT("Destroying dead Entity [%d] (no valid actor)"), Entity.Index);
            }
        }
    });
}