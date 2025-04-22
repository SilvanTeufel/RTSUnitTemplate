// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/CastingStateProcessor.h"

#include "Mass/UnitMassTag.h"


UCastingStateProcessor::UCastingStateProcessor()
{
}

void UCastingStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UCastingStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
