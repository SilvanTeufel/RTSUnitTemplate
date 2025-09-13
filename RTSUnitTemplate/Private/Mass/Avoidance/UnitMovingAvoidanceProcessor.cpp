// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Avoidance/UnitMovingAvoidanceProcessor.h"
#include "Mass/UnitMassTag.h"

UUnitMovingAvoidanceProcessor::UUnitMovingAvoidanceProcessor(): EntityQuery()
{
	// Execute in the same group as the old MassMovingAvoidanceProcessor
	ExecutionOrder.ExecuteInGroup = FName("Avoidance");

	// Execute after LOD and UnitMovementProcessor
	ExecutionOrder.ExecuteAfter.Add(FName("LOD"));
	ExecutionOrder.ExecuteAfter.Add(FName("UnitMovementProcessor"));

	// No need to execute before anything
	ExecutionOrder.ExecuteBefore.Empty();

	// Processing phase matches the original
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// Make sure the processor is auto-registered
	bAutoRegisterWithProcessingPhases = true;

	// Runs on Server + Client + Standalone (same as default)
	ExecutionFlags = static_cast<uint8>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);

	// Does not require Game Thread
	bRequiresGameThreadExecution = false;
}

void UUnitMovingAvoidanceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	// Call parent to set up all the standard requirements
	Super::ConfigureQueries(EntityManager);

	// Add requirement: only include entities that DO NOT have the disable tag
	EntityQuery.AddTagRequirement<FMassDisableAvoidanceTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}
