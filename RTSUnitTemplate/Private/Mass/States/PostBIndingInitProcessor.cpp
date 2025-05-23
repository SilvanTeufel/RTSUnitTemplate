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
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    // …and any others you need in InitAIFragments, etc…
    EntityQuery.RegisterWithProcessor(*this);
}

void UPostBindingInitProcessor::Execute(FMassEntityManager& EntityManager,
                                        FMassExecutionContext& Context)
{

}