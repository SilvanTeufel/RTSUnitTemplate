// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/Worker/GoToBaseStateProcessor.h"

#include "Mass/UnitMassTag.h"

UGoToBaseStateProcessor::UGoToBaseStateProcessor()
{
}

void UGoToBaseStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UGoToBaseStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
