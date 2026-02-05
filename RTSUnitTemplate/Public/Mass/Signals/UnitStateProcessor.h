// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
class AConstructionUnit;
struct FMassEntityManager;
struct FMassAIStateFragment;
struct FMassActorFragment; // Include MassActorFragment.h if needed

UCLASS()
class RTSUNITTEMPLATE_API UUnitStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitStateProcessor();

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = false;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	FVector BuildingSpawnTrace = FVector(0.f, 0.f, 2500.f);
protected:
	// We don't need ConfigureQueries or Execute for typical frame updates
	// We only need to register our signal handler
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	
	virtual void BeginDestroy() override;
private:
	UPROPERTY(Transient)
	UWorld* World;
	
	FMassEntityQuery EntityQuery;
	// Handler function for the signal (must match delegate signature)
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	// Thread-safe flags for shutdown/unbinding coordination
	FThreadSafeBool bDelegatesUnbound = false;
	// Set to true once BeginDestroy starts so handlers can early-out during teardown
	FThreadSafeBool bIsShuttingDown = false;

	// Helper functions to unbind signal delegates safely on the game thread
	void UnbindDelegates_Internal();
	void UnbindDelegates_GameThread();
	
	
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

	// Repair time sync handler (server-authoritative)
	UFUNCTION()
	void SyncRepairTime(FName SignalName, TArray<FMassEntityHandle>& Entities);

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
		// Worker routes
		UnitSignals::GoToBase,
		UnitSignals::GoToBuild,
		UnitSignals::Build,
		UnitSignals::GoToResourceExtraction,
		UnitSignals::ResourceExtraction,
		// Repair routes
		UnitSignals::GoToRepair,
		UnitSignals::Repair,
	};


	const TArray<FName> SightChangeSignals = {
		UnitSignals::UnitEnterSight,
		UnitSignals::UnitExitSight
		};
	// Delegate handle for unregistering
	TArray<FDelegateHandle> StateChangeSignalDelegateHandle;
	
	FDelegateHandle SyncUnitBaseDelegateHandle;
	
	FDelegateHandle SetUnitToChaseSignalDelegateHandle;
	FDelegateHandle RangedAbilitySignalDelegateHandle;
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
	FDelegateHandle SelectionCircleDelegateHandle;
	FDelegateHandle SpawnSignalDelegateHandle;

	
	// Cached subsystem pointers
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;

	UPROPERTY()
	AResourceGameMode* ResourceGameMode;

	
	UFUNCTION()
	void UnitActivateRangedAbilities(
	  FName SignalName,
	  TArray<FMassEntityHandle>& Entities
	);
	
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
	void SetAbilityEnabledByKey(AUnitBase* UnitBase, const FString& Key, bool bEnable);
	
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
								AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint, bool bDoGroundTrace = true, class AWorkArea* BuildArea = nullptr);


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SpawnWorkResource(EResourceType ResourceType, FVector Location, TSubclassOf<class AWorkResource> WRClass, AUnitBase* ActorToLockOn);

		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void DespawnWorkResource(AWorkResource* WorkResource);
	
	UFUNCTION()
	void SyncCastTime(
		FName SignalName,
		TArray<FMassEntityHandle>& Entities
	);

private:
	// Clean code helpers extracted from SyncCastTime
	// Returns true if the ConstructionUnit path handled the entity (skip worker path)
	bool HandleExtensionCastForConstructionUnit(FMassEntityManager& EntityManager, const FMassEntityHandle& Entity, FMassAIStateFragment* StateFrag, class AConstructionUnit* Construction);

	// Handles regular worker/building cast progress and optional construction-site support
	void HandleWorkerOrBuildingCastProgress(FMassEntityManager& EntityManager, const FMassEntityHandle& Entity, FMassAIStateFragment* StateFrag, class AUnitBase* UnitBase);

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
	void HandleUpdateSelectionCircle(FName SignalName, TArray<FMassEntityHandle>& Entities);
	
	UFUNCTION()
	void HandleUnitSpawnedSignal(FName SignalName, TArray<FMassEntityHandle>& Entities);
	
	FDelegateHandle UpdateWorkerMovementDelegateHandle;
		
	UFUNCTION()
	void UpdateWorkerMovement(FName SignalName, TArray<FMassEntityHandle>& Entities);
	
	UFUNCTION()
	void UpdateUnitMovement(FMassEntityHandle& Entity, AUnitBase* UnitBase);
	
	UFUNCTION()
	void UpdateUnitArrayMovement(FMassEntityHandle& Entity, AUnitBase* UnitBase);

	// Follow feature handlers
	UFUNCTION()
	void HandleUpdateFollowMovement(FName SignalName, TArray<FMassEntityHandle>& Entities);
	UFUNCTION()
	void HandleCheckFollowAssigned(FName SignalName, TArray<FMassEntityHandle>& Entities);
		
};