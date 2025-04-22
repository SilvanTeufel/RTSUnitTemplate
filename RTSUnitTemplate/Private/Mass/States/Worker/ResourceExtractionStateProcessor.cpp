// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/Worker/ResourceExtractionStateProcessor.h"
#include "Mass/UnitMassTag.h"

UResourceExtractionStateProcessor::UResourceExtractionStateProcessor()
{
}

void UResourceExtractionStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
