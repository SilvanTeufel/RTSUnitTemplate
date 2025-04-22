// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/Worker/GoToBuildStateProcessor.h"

#include "Mass/UnitMassTag.h"

UGoToBuildStateProcessor::UGoToBuildStateProcessor()
{
}

void UGoToBuildStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBuildStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
