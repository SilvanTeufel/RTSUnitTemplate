// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/CastingStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"


UCastingStateProcessor::UCastingStateProcessor(): EntityQuery()
{
	// Standard-Einstellungen, können angepasst werden
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void UCastingStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::All); // Nur für Entities in diesem Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);     // Timer lesen/schreiben
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);   // CastTime lesen

	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
	
	EntityQuery.RegisterWithProcessor(*this);
}

void UCastingStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UCastingStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // QUICK_SCOPE_CYCLE_COUNTER(STAT_UCastingStateProcessor_Execute);
	
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return; 
	}
	TimeSinceLastRun -= ExecutionInterval;

    // Get World and Signal Subsystem once before the loop
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager to get World
    if (!World) return;

	if (!SignalSubsystem) return;
    // Make a weak pointer copy for safe capture in the async task

    // --- List for Game Thread Signal Updates ---
    TArray<FMassSignalPayload> PendingSignals;

    EntityQuery.ForEachEntityChunk(Context,
        // Capture PendingSignals by reference. Do NOT capture LocalSignalSubsystem directly.
        [this, &PendingSignals](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        	
        if (NumEntities == 0) return; // Skip empty chunks
        	
        // Get required fragment views
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];

            // 3. Increment cast timer. This modification stays here.
            StateFrag.StateTimer += ExecutionInterval;

            PendingSignals.Emplace(Entity, UnitSignals::SyncCastTime);
            // 4. Check if cast time is finished

            if (StateFrag.StateTimer >= StatsFrag.CastTime) // Use >= for safety
            {
                // Queue the signal instead of sending it directly
                PendingSignals.Emplace(Entity, UnitSignals::EndCast); // Use your actual signal FName
                continue;
            }
        }
    }); // End ForEachEntityChunk


    // --- Schedule Game Thread Task to Send Queued Signals ---
    if (!PendingSignals.IsEmpty())
    {
    	if (SignalSubsystem)
    	{
    		TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystemPtr = SignalSubsystem;
    		// Capture the weak subsystem pointer and MOVE the pending signals list
    		AsyncTask(ENamedThreads::GameThread, [SignalSubsystemPtr, SignalsToSend = MoveTemp(PendingSignals)]()
			{
				// This lambda runs on the Game Thread

				// Check if the subsystem is still valid on the Game Thread
				if (UMassSignalSubsystem* StrongSignalSubsystem = SignalSubsystemPtr.Get())
				{
					for (const FMassSignalPayload& Payload : SignalsToSend)
					{
						// Check if the FName is valid before sending (good practice)
						if (!Payload.SignalName.IsNone())
						{
							// Send signal safely from the Game Thread
							StrongSignalSubsystem->SignalEntity(Payload.SignalName, Payload.TargetEntity);
						}
					}
				}
        	
				// else: Subsystem was destroyed before this task ran, signals are lost (usually acceptable)
			});
    	}
    }
}
