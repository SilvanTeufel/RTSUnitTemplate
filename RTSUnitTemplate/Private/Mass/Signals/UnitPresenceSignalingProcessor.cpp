#include "Mass/Signals/UnitPresenceSignalingProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Mass/Signals/MySignals.h" // Your signal definitions

UUnitPresenceSignalingProcessor::UUnitPresenceSignalingProcessor()
{
    // Run early in the frame, before detection.
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UUnitPresenceSignalingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    // Query for all entities that can be detected.
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassSightFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddTagRequirement<FMassStopUnitDetectionTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    
    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitPresenceSignalingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    UWorld* World = GetWorld();

    if (!World) return;
    
    if (!SignalSubsystem)
    {
        SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
        if (!SignalSubsystem) return;
    }

    TArray<FMassEntityHandle> AllUnits;
    AllUnits.Reserve(256); 

    EntityQuery.ForEachEntityChunk(Context, 
        [&AllUnits](FMassExecutionContext& ChunkContext)
    {
        // CORRECTED LINE: Use 'auto' to correctly deduce the type TConstArrayView<FMassEntityHandle>
        const auto Entities = ChunkContext.GetEntities();
        // Get the AIStateFragment and then StateFrag.CanDetect
        // This line works perfectly with a TConstArrayView
        AllUnits.Append(Entities.GetData(), Entities.Num());
    });

    if (!AllUnits.IsEmpty())
    {
        SignalSubsystem->SignalEntities(UnitSignals::UnitInDetectionRange, AllUnits);
    }
}
