// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/Worker/BuildStateProcessor.h"

#include "Mass/UnitMassTag.h"

UBuildStateProcessor::UBuildStateProcessor()
{
}

void UBuildStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
