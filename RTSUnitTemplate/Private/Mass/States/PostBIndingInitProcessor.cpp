// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "MassActorSubsystem.h"
#include "Mass/States/PostBindingInitProcessor.h"
#include "Mass/MassActorBindingComponent.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

UPostBindingInitProcessor::UPostBindingInitProcessor(): EntityQuery()
{
    bAutoRegisterWithProcessingPhases = true;
    ProcessingPhase               = EMassProcessingPhase::PostPhysics;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void UPostBindingInitProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.RegisterWithProcessor(*this);
}

void UPostBindingInitProcessor::Execute(FMassEntityManager& EntityManager,
                                        FMassExecutionContext& Context)
{

}