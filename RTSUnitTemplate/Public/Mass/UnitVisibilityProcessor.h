// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassEntityTypes.h"
#include "UnitVisibilityProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UUnitVisibilityProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitVisibilityProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UFUNCTION()
	void HandleVisibilitySignals(FName SignalName, TArray<FMassEntityHandle>& Entities);

private:
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	UPROPERTY(Transient)
	UWorld* World;

	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;
	float ExecutionInterval = 0.05f; 

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;

	FDelegateHandle VisibilitySignalDelegateHandle;
};
