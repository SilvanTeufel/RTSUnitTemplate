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
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);

    // Ensure it's one of our units (optional, but good practice)
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All); // Adjust tag if needed

    // Don't signal for dead units
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSignalingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // 1. Zeit akkumulieren
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();

    // 2. Prüfen, ob das Intervall erreicht wurde
    if (TimeSinceLastRun < ExecutionInterval)
    {
        // Noch nicht Zeit, diesen Frame überspringen
        return;
    }

    // 3. Timer zurücksetzen (Interval abziehen ist genauer als auf 0 setzen)
    TimeSinceLastRun -= ExecutionInterval;
     //UE_LOG(LogTemp, Log, TEXT("UUnitSignalingProcessor!!!"));
    if (!SignalSubsystem)
    {
        return;
    }

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();

            /*
            //UE_LOG(LogTemp, Log, TEXT("UUnitSignalingProcessor!!! NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            
            const FMassEntityHandle CurrentEntity = ChunkContext.GetEntity(i);
            const FMassCombatStatsFragment* TargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(CurrentEntity);
            const FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(CurrentEntity);
            
           const float Now = GetWorld()->GetTimeSeconds();
           if ((Now - StateFrag->BirthTime) < 1.0f )
           {
               continue;  // this entity is not yet “1 second old”
           }
            //if (TargetStatsFrag->Health > 0.f)
            {
                if (!SignalSubsystem)
                {
                     continue; // Handle missing subsystem
                }

                if (!DoesEntityHaveFragment<FTransformFragment>(EntityManager, CurrentEntity) ||
                !DoesEntityHaveFragment<FMassCombatStatsFragment>(EntityManager, CurrentEntity) ||
                !DoesEntityHaveFragment<FMassAIStateFragment>(EntityManager, CurrentEntity) ||
                !DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EntityManager, CurrentEntity))
                    continue;
                
                SignalSubsystem->SignalEntityDeferred(
                ChunkContext,
                UnitSignals::UnitInDetectionRange,
                CurrentEntity);
            }
        }
            */
    });
}

