// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/CastingStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"


UCastingStateProcessor::UCastingStateProcessor(): EntityQuery()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
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

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
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

            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SyncCastTime, Entity);
            }
            // 4. Check if cast time is finished

            if (StateFrag.StateTimer >= StatsFrag.CastTime) // Use >= for safety
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::EndCast, Entity);
                }
                continue;
            }
        }
    }); // End ForEachEntityChunk

}
