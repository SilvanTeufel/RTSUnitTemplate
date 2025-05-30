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
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Delegates/Delegate.h" // <-- Explicitly include this header
#include "GameModes/ResourceGameMode.h"
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
	UPROPERTY(Transient)
	UWorld* World;
	
	FMassEntityQuery EntityQuery;
	// Handler function for the signal (must match delegate signature)

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UFUNCTION()
	void IdlePatrolSwitcher(FName SignalName, TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void ForceSetPatrolRandomTarget(FMassEntityHandle& Entity);

	UFUNCTION()
	void ChangeUnitState(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);
	
	void SwitchState(FName SignalName, FMassEntityHandle& Entity, const FMassEntityManager& EntityManager);

	UFUNCTION() // Wichtig für Signal-Registrierung per Name oder Delegate
	void SyncUnitBase(FName SignalName, TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void SynchronizeStatsFromActorToFragment(FMassEntityHandle Entity);

	UFUNCTION()
	void SynchronizeUnitState(FMassEntityHandle Entity);
	
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
		UnitSignals::IsAttacked,
		UnitSignals::GoToBase,
		UnitSignals::GoToBuild,
		UnitSignals::Build,
		UnitSignals::GoToResourceExtraction,
		UnitSignals::ResourceExtraction,
	};


	const TArray<FName> SightChangeSignals = {
		UnitSignals::UnitEnterSight,
		UnitSignals::UnitExitSight
		};
	// Delegate handle for unregistering
	TArray<FDelegateHandle> StateChangeSignalDelegateHandle;
	
	FDelegateHandle SyncUnitBaseDelegateHandle;
	
	FDelegateHandle SetUnitToChaseSignalDelegateHandle;
	FDelegateHandle MeleeAttackSignalDelegateHandle;
	FDelegateHandle RangedAttackSignalDelegateHandle;
	FDelegateHandle StartDeadSignalDelegateHandle;
	FDelegateHandle EndDeadSignalDelegateHandle;
	FDelegateHandle IdlePatrolSwitcherDelegateHandle;

	FDelegateHandle UnitStatePlaceholderDelegateHandle;
	
	FDelegateHandle SyncCastTimeDelegateHandle;
	FDelegateHandle EndCastDelegateHandle;
	
	FDelegateHandle ReachedBaseDelegateHandle;
	FDelegateHandle GetResourceDelegateHandle;
	FDelegateHandle StartBuildActionDelegateHandle;
	FDelegateHandle SpawnBuildingRequestDelegateHandle;

	TArray<FDelegateHandle> SightChangeRequestDelegateHandle;
	FDelegateHandle FogParametersDelegateHandle;
	
	FDelegateHandle SpawnSignalDelegateHandle;
	// Cached subsystem pointers
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;

	UPROPERTY()
	AResourceGameMode* ResourceGameMode;
	
	UFUNCTION()
	void UnitMeeleAttack(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void UnitRangedAttack(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void SetUnitToChase(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);
	
	UFUNCTION()
	void HandleStartDead(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void HandleEndDead(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void HandleGetResource(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void HandleReachedBase(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void HandleGetClosestBaseArea(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void HandleSpawnBuildingRequest(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
		AExtendedControllerBase* ControllerBase;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		AUnitBase* SpawnSingleUnit(FUnitSpawnParameter SpawnParameter, FVector Location,
								AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint);


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SpawnWorkResource(EResourceType ResourceType, FVector Location, TSubclassOf<class AWorkResource> WRClass, AUnitBase* ActorToLockOn);

		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void DespawnWorkResource(AWorkResource* WorkResource);
	
	UFUNCTION()
	void SyncCastTime(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void EndCast(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

	UFUNCTION()
	void SetToUnitStatePlaceholder(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);


	UFUNCTION()
	void HandleSightSignals(FName SignalName, TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void HandleUpdateFogMask(FName SignalName, TArray<FMassEntityHandle>& Entities);
	
	UFUNCTION()
	void HandleUnitSpawnedSignal(FName SignalName, TArray<FMassEntityHandle>& Entities);


	FDelegateHandle UpdateWorkerMovementDelegateHandle;
	
	UFUNCTION()
	void UpdateWorkerMovement(FName SignalName, TArray<FMassEntityHandle>& Entities);
	
	UFUNCTION()
	void UpdateUnitMovement(FMassEntityHandle& Entity, AUnitBase* UnitBase);
};