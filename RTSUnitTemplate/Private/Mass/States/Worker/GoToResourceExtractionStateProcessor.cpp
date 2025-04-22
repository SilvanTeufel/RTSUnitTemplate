// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/Worker/GoToResourceExtractionStateProcessor.h"

#include "Mass/UnitMassTag.h"

UGoToResourceExtractionStateProcessor::UGoToResourceExtractionStateProcessor()
{
}

void UGoToResourceExtractionStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UGoToResourceExtractionStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
