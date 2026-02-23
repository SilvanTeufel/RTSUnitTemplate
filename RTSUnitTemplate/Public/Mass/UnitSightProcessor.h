// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
// UnitSightProcessor.h
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassEntityTypes.h" // for FMassEntityHandle
#include "Delegates/Delegate.h" // for FDelegateHandle
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

	// Handle sight-related signals directly in the SightProcessor so it can run on client and server
	UFUNCTION()
	void HandleSightSignals(FName SignalName, TArray<FMassEntityHandle>& Entities);

private:
	// Split execute paths for server and client
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	
	UPROPERTY(Transient)
	UWorld* World;
	
	// Main (server) query
	FMassEntityQuery EntityQuery;

	// Query for EffectAreas
	FMassEntityQuery EffectAreaQuery;
	
	float TimeSinceLastRun = 0.0f;
	const float ExecutionInterval = 0.2f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
	
	// store both enter/exit sight delegate handles
	TArray<FDelegateHandle> SightSignalDelegateHandles;

	// fog update signal delegate handle
	FDelegateHandle FogParametersDelegateHandle;

	// Handle fog mask updates on client (and server if needed)
	UFUNCTION()
	void HandleUpdateFogMask(FName SignalName, TArray<FMassEntityHandle>& Entities);
};