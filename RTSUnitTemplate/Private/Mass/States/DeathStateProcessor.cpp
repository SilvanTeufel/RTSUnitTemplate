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
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    // Optional: Fragment, um zu sehen, ob Effekte schon abgespielt wurden
    // EntityQuery.AddRequirement<FDeathEffectStateFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.RegisterWithProcessor(*this);
}

void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    
    // --- Get World and Signal Subsystem once ---
    UWorld* World = Context.GetWorld(); // Use Context consistently
    if (!World) return;

    UMassSignalSubsystem* LocalSignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!LocalSignalSubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("UDeathStateProcessor: Could not get SignalSubsystem!"));
        return;
    }
    // Make a weak pointer copy for safe capture in the async task
    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = LocalSignalSubsystem;


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference.
        // Do NOT capture LocalSignalSubsystem directly here.
        [&PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>(); // Mutable for stopping
        const auto AgentFragList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer
            FMassVelocityFragment& Velocity = VelocityList[i]; // Mutable for stopping
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAgentCharacteristicsFragment AgentFrag = AgentFragList[i];
            // --- Stop Movement ---
            Velocity.Value = FVector::ZeroVector; // Modification stays here

            // --- Update Timer ---
            const float PrevTimer = StateFrag.StateTimer; // Store previous timer value
            StateFrag.StateTimer += DeltaTime; // Modification stays here
 
            if (PrevTimer <= KINDA_SMALL_NUMBER) // Or PrevTimer == 0.0f if guaranteed
            {
                PendingSignals.Emplace(Entity, UnitSignals::StartDead);
            }

            // --- Despawn Check ---
            // Ensure DespawnTime is accessible (e.g., member variable 'this->DespawnTime')
            if (StateFrag.StateTimer >= AgentFrag.DespawnTime+1.f)
            {
                PendingSignals.Emplace(Entity, UnitSignals::EndDead);
                  // --- Defer Entity Destruction (Stays Here) ---
                 // ChunkContext.Defer().DestroyEntity(Entity);
            }
        }
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
        // Capture the weak subsystem pointer and move the pending signals list
        AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
        {
            // Check if the subsystem is still valid on the Game Thread
            if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
            {
                for (const FMassSignalPayload& Payload : SignalsToSend)
                {
                    // Check if the FName is valid before sending
                    if (!Payload.SignalName.IsNone())
                    {
                       // Send signal safely from the Game Thread using FName
                       StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                    }
                }
            }
        });
    }
    
}

/*
void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = Context.GetWorld(); 
    // UE_LOG(LogTemp, Log, TEXT("UDeathStateProcessor::Execute!"));
    
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
                // UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!SIGNEL TO START DEATH!!!!!!!!"));
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

*/