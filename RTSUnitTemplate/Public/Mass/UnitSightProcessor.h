// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
// UnitSightProcessor.h
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "UnitSightProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UUnitSightProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitSightProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	
	UPROPERTY(Transient)
	UWorld* World;
	
	FMassEntityQuery EntityQuery;
	
	float TimeSinceLastRun = 0.0f;
	const float ExecutionInterval = 0.2f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
	
	FDelegateHandle SignalDelegateHandle;
};