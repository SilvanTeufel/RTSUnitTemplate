// Fill out your copyright notice in the Description page of Project Settings.


#include "MassActorSubsystem.h"
#include "Mass/States/PostBindingInitProcessor.h"
#include "Mass/MassActorBindingComponent.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

UPostBindingInitProcessor::UPostBindingInitProcessor()
{
    bAutoRegisterWithProcessingPhases = true;
    ProcessingPhase               = EMassProcessingPhase::PostPhysics;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void UPostBindingInitProcessor::ConfigureQueries()
{
    // Only match entities that still have the one-shot init tag:
    EntityQuery.AddTagRequirement<FNeedsActorBindingInitTag>(EMassFragmentPresence::All);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    // …and any others you need in InitAIFragments, etc…
    EntityQuery.RegisterWithProcessor(*this);
}

void UPostBindingInitProcessor::Execute(FMassEntityManager& EntityManager,
                                        FMassExecutionContext& Context)
{
    /*
     
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; // Skip this frame
    }
    TimeSinceLastRun = 0.0f; // Reset timer

    UE_LOG(LogTemp, Log, TEXT("UPostBindingInitProcessor::Execute started"));
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& SubCtx)
    {
        FMassCommandBuffer& CmdBuffer = SubCtx.Defer();
        const TArrayView<const FMassEntityHandle> Handles = SubCtx.GetEntities();
        TArrayView<FMassActorFragment> ActorFragments = Context.GetMutableFragmentView<FMassActorFragment>();
        UE_LOG(LogTemp, Warning, TEXT("Handles.Num(): %d"), Handles.Num());
        for (int32 i = 0; i < Handles.Num(); ++i)
        {
            FMassEntityHandle E = Handles[i];
            
            AActor* Actor = ActorFragments[i].GetMutable();
            if (!Actor)
            {
                UE_LOG(LogTemp, Warning, TEXT("NO ACTOR FOUND!!!"));
                continue;
            }

            UMassActorBindingComponent* Comp = Actor->FindComponentByClass<UMassActorBindingComponent>();
            if (Comp)
            {
                //Comp->InitTransform(EntityManager, E);
                //Comp->InitMovementFragments(EntityManager, E);
                //Comp->InitAIFragments(EntityManager, E);
                //Comp->InitRepresentation(EntityManager, E);
            }

            CmdBuffer.RemoveTag<FNeedsActorBindingInitTag>(E);
            UE::Mass::Debug::LogEntityTags(E, EntityManager);
        }
    });

    */
}