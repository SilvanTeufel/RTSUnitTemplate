// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Signals/UnitSignalingProcessor.h"

#include "MassSignalSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h" // Assuming you have a tag identifying your units
#include "Mass/Signals/MySignals.h" // Include the signal definition
#include "MassStateTreeFragments.h" // For FMassStateDeadTag
#include "MassNavigationFragments.h" // Assuming FMassAgentCharacteristicsFragment might be here or similar


const FName UUnitSignalingProcessor::UnitPresenceSignalName = FName("UnitPresence");

UUnitSignalingProcessor::UUnitSignalingProcessor()
{
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UUnitSignalingProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>();
}

void UUnitSignalingProcessor::ConfigureQueries()
{
    // Query entities that should announce their presence
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

    // Ensure it's one of our units (optional, but good practice)
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All); // Adjust tag if needed

    // Don't signal for dead units
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSignalingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
  
    if (!SignalSubsystem)
    {
        UE_LOG(LogMass, Error, TEXT("%s: MassSignalSubsystem is missing."), *GetName());
        return;
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        // const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
        // const auto Stats = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        // const auto Characteristics = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);

            // Create the signal data payload
            //FUnitPresenceSignal SignalData;
            //SignalData.SignalerEntity = CurrentEntity;
            //SignalData.Location = Transforms[i].GetTransform().GetLocation(); // Use GetLocation()
            //SignalData.TeamId = Stats[i].TeamId;
            //SignalData.bIsInvisible = Characteristics[i].bIsInvisible;
            //SignalData.bIsFlying = Characteristics[i].bIsFlying;

            // Broadcast the signal
            // The SignalSubsystem automatically handles spatial partitioning
            //SignalSubsystem->SignalEntity(UnitPresenceSignalName, CurrentEntity, SignalData);

            if (!SignalSubsystem)
            {
                 continue; // Handle missing subsystem
            }
            
            SignalSubsystem->SignalEntityDeferred(
            ChunkContext,
            UnitSignals::UnitInDetectionRange,
            CurrentEntity);
        }
    });
}