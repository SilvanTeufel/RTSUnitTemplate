// Header: UnitStateProcessor.h
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonTypes.h"   // For FMassEntityHandle
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h" // For signal delegates if using UFUNCTION handler
#include "MySignals.h"    // Include your signal definition header
#include "UnitStateProcessor.generated.h"

// Forward declarations
class UMassSignalSubsystem;
class AUnitBase;
struct FMassActorFragment; // Include MassActorFragment.h if needed

UCLASS()
class RTSUNITTEMPLATE_API UUnitStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitStateProcessor();

protected:
	// We don't need ConfigureQueries or Execute for typical frame updates
	// We only need to register our signal handler
	virtual void Initialize(UObject& Owner) override;
	virtual void BeginDestroy() override;

	virtual void ConfigureQueries() override;
private:
	// Handler function for the signal (must match delegate signature)
	void OnUnitReachedDestination(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);

	// Delegate handle for unregistering
	FDelegateHandle ReachedDestinationSignalDelegateHandle;

	// Cached subsystem pointers
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem; // Needed to get fragments
};