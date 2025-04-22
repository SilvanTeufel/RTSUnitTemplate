// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/States/IsAttackedStateProcessor.h"
#include "Mass/UnitMassTag.h"


UIsAttackedStateProcessor::UIsAttackedStateProcessor()
{
}

void UIsAttackedStateProcessor::ConfigureQueries()
{
	//Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UIsAttackedStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	//UE_LOG(LogTemp, Warning, TEXT("URunStateProcessor!"));
}
