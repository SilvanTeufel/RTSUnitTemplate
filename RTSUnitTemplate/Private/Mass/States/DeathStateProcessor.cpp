// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
#include "Async/Async.h"


UDeathStateProcessor::UDeathStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UDeathStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::All); // Nur tote Entitäten
    
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Anhalten
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.RegisterWithProcessor(*this);
}

void UDeathStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // --- Get World and Signal Subsystem once ---
    UWorld* World = Context.GetWorld(); // Use Context consistently
    if (!World) return;

    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Mutable for timer
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>(); // Mutable for stopping
        const auto AgentFragList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAgentCharacteristicsFragment CharacteristicsFragment = AgentFragList[i];

            Velocity.Value = FVector::ZeroVector;


            const float PrevTimer = StateFrag.StateTimer;
            StateFrag.StateTimer += ExecutionInterval;
 
            if (PrevTimer <= KINDA_SMALL_NUMBER)
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::StartDead, Entity);
                }
            }
            
            if (StateFrag.StateTimer >= CharacteristicsFragment.DespawnTime+1.f)
            {
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::EndDead, Entity);
                }
            }
        }
    }); // End ForEachEntityChunk


    
}
