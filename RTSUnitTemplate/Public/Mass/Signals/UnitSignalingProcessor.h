// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "UnitSignalingProcessor.generated.h"


/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitSignalingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitSignalingProcessor();

protected:
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	// Static name for the signal type for consistency
	static const FName UnitPresenceSignalName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ExecutionInterval = 0.2f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};