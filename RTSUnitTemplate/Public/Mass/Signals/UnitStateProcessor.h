// Header: UnitStateProcessor.h
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonTypes.h"   // For FMassEntityHandle
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h" // For signal delegates if using UFUNCTION handler
#include "MySignals.h"    // Include your signal definition header
#include "MassEntityTypes.h" // For FMassEntityHandle
#include "UObject/ObjectMacros.h" // Required for UFUNCTION
#include "Containers/ArrayView.h" // Ensure TConstArrayView/TArrayView is available
#include "Delegates/Delegate.h" // <-- Explicitly include this header
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
	FMassEntityQuery EntityQuery;
	// Handler function for the signal (must match delegate signature)

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	UFUNCTION()
	void ChangeUnitState(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);

	const TArray<FName> StateChangeSignals = {
		UnitSignals::Idle,
		UnitSignals::Chase,
		UnitSignals::Attack,
		UnitSignals::Dead,        // Or handle death differently
		UnitSignals::PatrolIdle,
		UnitSignals::PatrolRandom,
		UnitSignals::Pause,
		UnitSignals::Run,
		UnitSignals::Casting,
	};
	
	// Delegate handle for unregistering
	TArray<FDelegateHandle> StateChangeSignalDelegateHandle;

	FDelegateHandle MeleeAttackSignalDelegateHandle;
	FDelegateHandle RangedAttackSignalDelegateHandle;
	// Cached subsystem pointers
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem; // Needed to get fragments


	UFUNCTION()
	void UnitMeeleAttack(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);
	
	void UnitRangedAttack(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);
};