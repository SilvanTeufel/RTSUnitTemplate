// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "GoToBuildStateProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UGoToBuildStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGoToBuildStateProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	// Must match UUnitStateProcessor::ArrivalDistanceMultiplier so client-predicted stop point
	// lands where the server actually halts (server uses this * MovementAcceptanceRadius).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ArrivalDistanceMultiplier = 5.f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;
	// No configuration properties needed here now.
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};