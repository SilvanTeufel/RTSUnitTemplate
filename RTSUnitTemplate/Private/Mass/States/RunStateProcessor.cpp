// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/RunStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"

URunStateProcessor::URunStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void URunStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All); // Nur Entities im Run-Zustand
    
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);       // Aktuelle Position lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel-Daten lesen, Stoppen erfordert Schreiben
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
 TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;
    // Get World and Signal Subsystem once
    UWorld* World = Context.GetWorld(); // Use Context to get World
    if (!World) return;

    if (!SignalSubsystem) return;
    
    EntityQuery.ForEachEntityChunk(Context,

        [this, World, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>(); // Keep mutable if State needs updates
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        auto MoveTargetList = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>(); // Mutable for Update/Stop
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i]; // Keep reference if State needs updates
            FMassMoveTargetFragment& MoveTarget = MoveTargetList[i]; // Mutable for Update/Stop
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            
            const FTransform& CurrentMassTransform = TransformList[i].GetTransform();
            const FVector CurrentLocation = CurrentMassTransform.GetLocation();
            const FVector FinalDestination = MoveTarget.Center;
            const float AcceptanceRadius = MoveTarget.SlackRadius; //150.f;

            StateFrag.StateTimer += ExecutionInterval;
            
            if (DoesEntityHaveTag(EntityManager,Entity, FMassStateDetectTag::StaticStruct()) &&
                TargetFrag.bHasValidTarget && !StateFrag.SwitchingState)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Chase, Entity);
                }
            }else if (FVector::Dist(CurrentLocation, FinalDestination) <= AcceptanceRadius)
            {
                StateFrag.SwitchingState = true;
                if (SignalSubsystem)
                {
                    SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::Idle, Entity);
                }
                continue;
            }
            
        }
    }); // End ForEachEntityChunk


}
