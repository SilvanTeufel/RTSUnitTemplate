// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/RunStateProcessor.h"

#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"

URunStateProcessor::URunStateProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
}

void URunStateProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All); // Nur Entities im Run-Zustand

	// Benötigte Fragmente:
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);       // Aktuelle Position lesen
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // Ziel-Daten lesen, Stoppen erfordert Schreiben
	
	// Schließe tote Entities aus
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

}
