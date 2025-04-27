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

struct FMassSignalPayload
{
	FMassEntityHandle TargetEntity;
	FName SignalName; // Use FName for the signal identifier

	// Constructor using FName
	FMassSignalPayload(FMassEntityHandle InEntity, FName InSignalName)
		: TargetEntity(InEntity), SignalName(InSignalName)
	{}
};

void UPatrolRandomStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling Check ---
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
       return; // Skip execution this frame
    }
    // Interval reached, reset timer
    TimeSinceLastRun -= ExecutionInterval; // Or TimeSinceLastRun = 0.0f;

    // --- Get World and Signal Subsystem (only if interval was met) ---
    // Use Context or EntityManager to get World consistently
    UWorld* World = Context.GetWorld();
    if (!World) return;

    UMassSignalSubsystem* LocalSignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!LocalSignalSubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("UPatrolRandomStateProcessor: Could not get SignalSubsystem!"));
        return;
    }
    // Make a weak pointer copy for safe capture in the async task
    TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = LocalSignalSubsystem;


    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;
    // PendingSignals.Reserve(ExpectedSignalCount); // Optional

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingSignals by reference. Capture World for StopMovement.
        // Do NOT capture LocalSignalSubsystem directly here.
        [&PendingSignals, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer reset
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for StopMovement
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i]; // Mutable for timer reset
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for StopMovement
            const FTransformFragment& TransformFrag = TransformList[i];

            // --- 1. Check for sighted enemy ---
            if (TargetFrag.bHasValidTarget)
            {
                // Queue signal instead of sending directly
                PendingSignals.Emplace(Entity, UnitSignals::Chase);

                StateFrag.StateTimer = 0.f; // Reset timer immediately (modifies fragment)
                // Maybe stop movement here too? StopMovement(MoveTarget, World);
                continue; // Switch state, process next entity
            }

            // --- 2. Check if current patrol destination reached ---
            // Only check if actually moving towards a destination
            if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move)
            {
                const FVector CurrentLocation = TransformFrag.GetTransform().GetLocation();
                const FVector CurrentDestination = MoveTarget.Center;
                const float AcceptanceRadius = MoveTarget.SlackRadius * 4; // Consider making multiplier a variable
                const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);
                const float DistSq = FVector::DistSquared(CurrentLocation, CurrentDestination);

                if (DistSq <= AcceptanceRadiusSq) // --- Destination Reached ---
                {
                    // Queue signal instead of sending directly
                    PendingSignals.Emplace(Entity, UnitSignals::PatrolIdle);

                    StateFrag.StateTimer = 0.f; // Reset timer immediately (modifies fragment)

                    // Stop movement immediately (modifies fragment)
                    StopMovement(MoveTarget, World);

                    continue; // Switch state, process next entity
                }
                // --- Else: Still moving towards destination, do nothing else ---
            }
            // --- Else: Not currently moving (e.g., Action is Stand), do nothing ---

        } // End Entity Loop
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
void UPatrolRandomStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Intervall-Prüfung (wie in deinem Code) ---
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
       return;
    }
    TimeSinceLastRun -= ExecutionInterval;
    // -------------------------------------------

    UWorld* World = GetWorld(); // Welt holen
    if (!World) return;

    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Erfasse World für StopMovement und SignalSubsystem
        [this, SignalSubsystem, World](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        // auto PatrolList = ChunkContext.GetMutableFragmentView<FMassPatrolFragment>(); // Wird hier nicht direkt gebraucht
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable für StopMovement
        // const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>(); // Wird hier nicht direkt gebraucht
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            // FMassPatrolFragment& PatrolFrag = PatrolList[i];
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable Referenz
            // const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransformFragment& TransformFrag = TransformList[i];

            // UE_LOG(LogTemp, Log, TEXT("PatrolRandom Entity %d:%d processing."), Entity.Index, Entity.SerialNumber);

            // --- 1. Prüfen, ob Feind gesichtet wurde ---
            // (Diese Logik hattest du schon)
            if (TargetFrag.bHasValidTarget)
            {
                SignalSubsystem->SignalEntity(UnitSignals::Chase, Entity);
                StateFrag.StateTimer = 0.f; // Timer für neuen Zustand zurücksetzen
                continue; // Zustand wechseln, weiter zur nächsten Entity
            }

            // --- 2. Prüfen, ob das aktuelle Patrouillenziel erreicht wurde ---
            // Nur prüfen, wenn die aktuelle Aktion "Move" ist
            if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move)
            {
                const FVector CurrentLocation = TransformFrag.GetTransform().GetLocation();
                const FVector CurrentDestination = MoveTarget.Center;
                const float AcceptanceRadius = MoveTarget.SlackRadius*4;
                const float AcceptanceRadiusSq = FMath::Square(AcceptanceRadius);
                const float DistSq = FVector::DistSquared(CurrentLocation, CurrentDestination);

                if (DistSq <= AcceptanceRadiusSq)
                {
                    // Signalisiere den PatrolIdle Zustand
                    SignalSubsystem->SignalEntity(UnitSignals::PatrolIdle, Entity);

                    // Setze den Timer für den neuen Zustand zurück
                    StateFrag.StateTimer = 0.f;

                    // Stoppe die aktuelle Bewegung explizit (gute Praxis)
                    // Annahme: StopMovement existiert und setzt die Aktion auf Stand
                    StopMovement(MoveTarget, World);

                    // Fahre mit der nächsten Entity fort, da diese den Zustand wechselt
                    continue;
                }
            }
        } // Ende Entity Loop
    }); // Ende ForEachEntityChunk
}*/