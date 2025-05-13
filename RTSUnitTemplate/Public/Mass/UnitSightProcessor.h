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
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	
private:



	UPROPERTY(Transient)
	UWorld* World;
	
	void HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	
	FMassEntityQuery EntityQuery;

	// Timer, um diesen Prozessor nicht jeden Frame laufen zu lassen
	// Besser über ProcessorGroup Konfiguration steuern! Dies ist nur ein Beispiel.
	float TimeSinceLastRun = 0.0f;
	const float ExecutionInterval = 0.2f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
	
	// Handle for the bound delegate, used for unbinding (though standard unbinding might be tricky)
	FDelegateHandle SignalDelegateHandle;
	//FDelegateHandle SpawnSignalDelegateHandle;
	// Buffer to store entities received from the signal delegate between calls
	// Key: Signal Name, Value: Array of Entities that signaled
	TMap<FName, TArray<FMassEntityHandle>> ReceivedSignalsBuffer;

	// Keep track of processed entities from buffer to handle target loss check correctly
	TSet<FMassEntityHandle> SignaledEntitiesProcessedThisTick;
};