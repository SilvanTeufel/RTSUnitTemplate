// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/RunStateProcessor.h"

#include "Mass/UnitMassTag.h"

URunStateProcessor::URunStateProcessor()
{
}

void URunStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
