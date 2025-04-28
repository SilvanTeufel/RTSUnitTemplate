#include "Mass/States/AttackStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h" // Für Schadens-Signal

// Fragmente und Tags
#include "MassActorSubsystem.h"   
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassActorSubsystem.h"  
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"


UAttackStateProcessor::UAttackStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UAttackStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UAttackStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None); // Exclude Chase too
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None); // Already excluded by other logic, but explicit

    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Ensure the member SignalSubsystem is valid (initialized in Initialize)
    if (!SignalSubsystem)
    {
        // Log error or attempt reinitialization if appropriate
        return;
    }
    // Make a weak pointer copy for safe capture in the async task
    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference, DO NOT capture SignalSubsystem directly
        [&PendingSignals](FMassExecutionContext& ChunkContext) // Removed SignalSubsystem capture here
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>(); // Keep mutable if velocity modification is intended later
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();

        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // State modification stays here
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassVelocityFragment& Velocity = VelocityList[i]; // Keep reference if velocity modification is intended later

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // --- Target Lost ---
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                // Queue signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Run); // Adjust UnitSignals::Run based on payload struct
                continue;
            }

            // --- Attack Cycle Timer ---
            StateFrag.StateTimer += DeltaTime; // State modification stays here

            if (StateFrag.StateTimer <= Stats.AttackDuration)
            {
                // --- Range Check ---
                const float EffectiveAttackRange = Stats.AttackRange;
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    // --- Melee Impact Check ---
                    if (!Stats.bUseProjectile && !StateFrag.HasAttacked)
                    {
                        // Queue signal instead of sending directly
                        PendingSignals.Emplace(Entity, UnitSignals::MeleeAttack); // Adjust UnitSignals::MeleeAttack
                        StateFrag.HasAttacked = true; // State modification stays here
                        // Note: We queue the signal but modify state immediately. This might
                        // slightly change behavior if the signal was expected to trigger
                        // something *before* HasAttacked was set. Usually okay.
                    }
                    // else if (Stats.bUseProjectile && !StateFrag.HasAttacked) { /* Handle projectile signal queuing */ }
                }
                else // --- Target moved out of range ---
                {
                     // Queue signal instead of sending directly
                    PendingSignals.Emplace(Entity, UnitSignals::Chase); // Adjust UnitSignals::Chase
                    continue;
                }
            }
            else // --- Attack Duration Over ---
            {
                 // Queue signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Pause); // Adjust UnitSignals::Pause
                StateFrag.HasAttacked = false; // State modification stays here
                continue;
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
                    // Check Payload validity if needed (e.g., if SignalStruct could be null)
                    if (!Payload.SignalName.IsNone()) // Or check based on your signal type
                    {
                       // Send signal safely from the Game Thread
                       StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                       // Or use the appropriate SignalEntity overload based on your signal type
                       // StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
                       // StrongSignalSubsystem->SignalEntity(Payload.SignalID, Payload.TargetEntity);
                    }
                }
            }
            // else: Subsystem was destroyed before the task could run, signals are lost (usually acceptable)
        });
    }
}

/*
void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Log, TEXT("UAttackStateProcessor::Execute!"));
    UWorld* World = Context.GetWorld(); // World für MoveTarget holen

    // Stelle sicher, dass das Signal Subsystem gültig ist
    if (!SignalSubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("UAttackStateProcessor: SignalSubsystem is invalid!"));
        return;
    }

    // Set leeren, das speichert, wer in diesem Tick schon angegriffen hat
   //EntitiesThatAttackedThisTick.Reset();

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {

        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
 
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        //UE_LOG(LogTemp, Log, TEXT("Attack EntityCount:! %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassVelocityFragment& Velocity = VelocityList[i];
        

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            
            //UE::Mass::Debug::LogEntityTags(Entity, EntityManager, this);

            // 1. Sicherstellen, dass Einheit steht
            //Velocity.Value = FVector::ZeroVector;
            // Rotation zum Ziel: Sollte idealerweise ein separater LookAtProcessor machen
            // 2. Ziel verloren oder ungültig? -> Zurück zu Idle/Chase
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                //UE_LOG(LogTemp, Error, TEXT("TARGET NOT VALID ANYMORE!!!!!!!"));
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem) continue;
                    
                SignalSubsystem->SignalEntity(UnitSignals::Run, Entity);
                continue;
            }

            // 3. Timer für Angriffszyklus prüfen
            StateFrag.StateTimer += DeltaTime;

            // Gesamtdauer des Angriffs (ohne Pause)
            // 4. Prüfen, ob der "Impact"-Zeitpunkt erreicht ist und wir noch nicht angegriffen haben
            if (StateFrag.StateTimer <= Stats.AttackDuration) //&& !EntitiesThatAttackedThisTick.Contains(Entity)
            {
                // Prüfen, ob Ziel noch in Reichweite ist
                const float EffectiveAttackRange = Stats.AttackRange; // + Stats.AgentRadius; // Vereinfacht
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    if (!Stats.bUseProjectile && !StateFrag.HasAttacked)// Nahkampf
                    {
                        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                        if (!SignalSubsystem) continue;
                        
                        SignalSubsystem->SignalEntity(UnitSignals::MeleeAttack, Entity);
                        StateFrag.HasAttacked = true;
                    }
                }
                else
                {
                    //UE_LOG(LogTemp, Log, TEXT("Attack TO CHASE!!!!!!!"));
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
            }else
            {
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem)
                {
                   continue; // Handle missing subsystem
                }
                SignalSubsystem->SignalEntity(
                UnitSignals::Pause,
                Entity);
                // Angriff beendet -> Wechsle zu Pause
                StateFrag.HasAttacked = false;
                continue;
            }

            // 6. Im Attack-State bleiben
        }
    });
}
*/