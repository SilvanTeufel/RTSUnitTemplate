// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassSignalTypes.h"
#include "DetectionProcessor.generated.h"

struct FMassExecutionContext;
struct FTransformFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassAgentCharacteristicsFragment;

UCLASS()
class RTSUNITTEMPLATE_API UDetectionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UDetectionProcessor();

protected:
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	// This is where you respond to the signal
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager,
								FMassExecutionContext& Context,
								FMassSignalNameLookup& EntitySignals);

private:

	void HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);

	
	FMassEntityQuery EntityQuery;

	// Timer, um diesen Prozessor nicht jeden Frame laufen zu lassen
	// Besser über ProcessorGroup Konfiguration steuern! Dies ist nur ein Beispiel.
	float TimeSinceLastRun = 0.0f;
	const float DetectionInterval = 0.2f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	// Handle for the bound delegate, used for unbinding (though standard unbinding might be tricky)
	FDelegateHandle SignalDelegateHandle;

	// Buffer to store entities received from the signal delegate between calls
	// Key: Signal Name, Value: Array of Entities that signaled
	TMap<FName, TArray<FMassEntityHandle>> ReceivedSignalsBuffer;

	// Keep track of processed entities from buffer to handle target loss check correctly
	TSet<FMassEntityHandle> SignaledEntitiesProcessedThisTick;
};