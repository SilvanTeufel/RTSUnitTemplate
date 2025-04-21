#include "Mass/States/DeathStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"  
#include "MassCommonFragments.h" // Für FMassRepresentationFragment (optional für Sichtbarkeit)

// Für Actor Cast und Destroy
#include "Characters/Unit/UnitBase.h"


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
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite); // Actor für Effekte/Destroy

    // Optional: Fragment, um zu sehen, ob Effekte schon abgespielt wurden
    // EntityQuery.AddRequirement<FDeathEffectStateFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.RegisterWithProcessor(*this);
}

void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UE_LOG(LogTemp, Log, TEXT("UDeathStateProcessor::Execute!"));
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
            
        TArrayView<FMassActorFragment> ActorFragments = Context.GetMutableFragmentView<FMassActorFragment>(); 
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];
            
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor); // Cast, falls spezifische Logik nötig

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1. Bewegung stoppen
            Velocity.Value = FVector::ZeroVector;

            // 2. Timer hochzählen
            StateFrag.StateTimer += DeltaTime;

            // 3. Effekte beim ersten Eintritt in Dead State auslösen (Beispiel)
            if (StateFrag.StateTimer <= DeltaTime && UnitBase) // Nur im ersten Frame dieses Zustands
            {
                // TODO: Feuere DeadVFX und DeadSound über den Actor
                // UnitBase->FireEffects(...);
                 UnitBase->HideHealthWidget(); // Aus deinem Code
                 UnitBase->KillLoadedUnits();
                 UnitBase->CanActivateAbilities = false;
                 UnitBase->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                 ActorFragments[i].GetMutable()->SetActorEnableCollision(false); // Kollision am Actor deaktivieren
            }


            // 4. Despawn / Destroy nach Zeit
            if (StateFrag.StateTimer >= DespawnTime)
            {
                 if (UnitBase)
                 {
                    // UnitBase->UnitWillDespawn(); // Deine Funktion aufrufen
                    // UnitBase->SpawnPickupsArray();
                 }
                
                 // Option B: Den Actor zerstören (was oft auch die Entity entfernt, je nach Setup)
                 if (ActorFragments[i].IsValid())
                 {
                     AActor* ActorToDestroy =  ActorFragments[i].GetMutable(); // Holen vor Reset
                     //ActorFrag.Reset(); // Verknüpfung in Mass lösen
                     if (IsValid(ActorToDestroy))
                     {
                         // TODO: Prüfe UnitBase->DestroyAfterDeath Flag
                         ActorToDestroy->Destroy();
                         // UE_LOG(LogTemp, Log, TEXT("Destroying Actor for dead Entity [%d]"), Entity.Index);
                     }
                 } else {
                      // Nur Entity zerstören, wenn kein Actor (mehr) da ist
                       ChunkContext.Defer().DestroyEntity(Entity);
                       // UE_LOG(LogTemp, Log, TEXT("Destroying dead Entity [%d] (no valid actor)"), Entity.Index);
                 }
            }
        }
    });
}