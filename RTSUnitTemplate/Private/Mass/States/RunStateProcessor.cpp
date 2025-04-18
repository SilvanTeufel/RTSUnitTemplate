// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/RunStateProcessor.h"

URunStateProcessor::URunStateProcessor()
{
}

void URunStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.RegisterWithProcessor(*this);
}

void URunStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	
}
