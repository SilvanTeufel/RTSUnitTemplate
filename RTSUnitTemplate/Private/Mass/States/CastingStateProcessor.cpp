// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/CastingStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"


UCastingStateProcessor::UCastingStateProcessor(): EntityQuery()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Client | (int32)EProcessorExecutionFlags::Standalone;
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
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite);     // Rotate flag setzen/zurücksetzen

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

    UWorld* World = Context.GetWorld();
    if (!World)
    {
        return;
    }

    if (World->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UCastingStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Mirror server checks on client and update local-only flags/state
    UWorld* World = Context.GetWorld();
    if (!World) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        if (NumEntities == 0) return;

        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            FMassAITargetFragment& TargetFrag = TargetList[i];

            // Set rotation flag at cast start
            if (StateFrag.StateTimer <= 0.2f)
            {
                TargetFrag.bRotateTowardsAbility = true;
            }

            // Increase the timer by the processor interval
            StateFrag.StateTimer += ExecutionInterval;

            // End of cast on client: clear rotation flag
            if (StateFrag.StateTimer >= StatsFrag.CastTime)
            {
                TargetFrag.bRotateTowardsAbility = false;
                // No signals on client; server will drive authoritative transitions
                continue;
            }
        }
    });
}

void UCastingStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Get World and Signal Subsystem once before the loop
    UWorld* World = EntityManager.GetWorld(); // Use EntityManager to get World
    if (!World) return;

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

        if (NumEntities == 0) return; // Skip empty chunks

        // Get required fragment views
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto TargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassCombatStatsFragment& StatsFrag = StatsList[i];
            FMassAITargetFragment& TargetFrag = TargetList[i];

            // At cast start, set rotate towards ability flag
            if (StateFrag.StateTimer <= 0.2f)
            {
                TargetFrag.bRotateTowardsAbility = true;
            }

            // 3. Increment cast timer. This modification stays here.
            StateFrag.StateTimer += ExecutionInterval;

            if (SignalSubsystem)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::SyncCastTime, Entity);
            }
            // 4. Check if cast time is finished

            if (StateFrag.StateTimer >= StatsFrag.CastTime) // Use >= for safety
            {
                // Clear rotate flag at end of cast
                TargetFrag.bRotateTowardsAbility = false;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::EndCast, Entity);
                }
                continue;
            }
        }
    }); // End ForEachEntityChunk
}
