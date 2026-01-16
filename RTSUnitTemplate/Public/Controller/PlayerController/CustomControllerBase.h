// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"


#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassCommandBuffer.h"
#include "MassNavigationFragments.h"
#include "MassMovementFragments.h"
#include "Actors/SelectionCircleActor.h"
#include "Engine/World.h"        // Include for UWorld, GEngine
#include "Engine/Engine.h"       // Include for GEngine
#include "Engine/EngineTypes.h"   // For FHitResult in UFUNCTION params
#include "TimerManager.h"  // For FTimerHandle

class USoundBase;
class AUnitBase;

#include "CustomControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACustomControllerBase : public AExtendedControllerBase
{
	GENERATED_BODY()
protected:
	// /** If true, the formation will be recalculated on the next move command, even if the selection hasn't changed. */
	// UPROPERTY(BlueprintReadWrite, Category = "RTS")
	bool bForceFormationRecalculation = true;
	// 
	// /** Stores the calculated offset for each unit from the formation's center point. This preserves the formation shape. */
	TMap<AUnitBase*, FVector> UnitFormationOffsets;
	// 
	// /** A snapshot of the last group of units for which a formation was calculated. Used to detect changes in selection. */
	TArray<TWeakObjectPtr<AUnitBase>> LastFormationUnits;

	// Retry state for deferred follow-target commands
	FTimerHandle FollowRetryTimerHandle;
	int32 FollowRetryRemaining = 0;
	TArray<TWeakObjectPtr<AUnitBase>> PendingFollowUnits;
	TWeakObjectPtr<AUnitBase> PendingFollowTarget;
	bool PendingFollowAttackT = false;

	// Helpers for follow-target deferral
	bool IsFollowCommandReady(const TArray<AUnitBase*>& Units);
	void ScheduleFollowRetry(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT, int32 MaxAttempts = 8, float DelaySeconds = 0.5f);
	void Retry_Server_SetUnitsFollowTarget();
	void ExecuteFollowCommand(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT);

	// Handles follow command on right-click. Returns true if a follow action was issued (and should early return)
	bool TryHandleFollowOnRightClick(const FHitResult& HitPawn);

public:
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetMyTeamUnits(const TArray<AActor*>& AllUnits);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetCamLocation(FVector NewLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_HideEnemyWaypoints();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_InitFogOfWar();

	
	UFUNCTION(Client, Reliable)
	void AgentInit();
	
	UFUNCTION(Server, Reliable, BlueprintCallable,  Category = RTSUnitTemplate)
	void CorrectSetUnitMoveTarget(
		UObject* WorldContextObject,
		AUnitBase* Unit,
		const FVector& NewTargetLocation,
		float DesiredSpeed = 300.0f,
		float AcceptanceRadius = 50.0f,
		bool AttackT = false);

	// Assign or clear a follow target for a set of units on the server.
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_SetUnitsFollowTarget(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT = false);

	// Batched version to reduce per-unit RPC spamming when issuing group move orders
	// Now multicast so that all clients receive the movement updates, but invoked by a server wrapper
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Batch_CorrectSetUnitMoveTargets(
		UObject* WorldContextObject,
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& NewTargetLocations,
		const TArray<float>& DesiredSpeeds,
		const TArray<float>& AcceptanceRadii,
		bool AttackT = false);

	// Server wrapper to validate and then trigger the multicast from the authority
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_Batch_CorrectSetUnitMoveTargets(
		UObject* WorldContextObject,
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& NewTargetLocations,
		const TArray<float>& DesiredSpeeds,
		const TArray<float>& AcceptanceRadii,
		bool AttackT = false);

	// Client-side prediction: apply Run tag and local MoveTarget updates on each client
	UFUNCTION(Client, Reliable)
	void Client_Predict_Batch_CorrectSetUnitMoveTargets(
		UObject* WorldContextObject,
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& NewTargetLocations,
		const TArray<float>& DesiredSpeeds,
		const TArray<float>& AcceptanceRadii,
		bool AttackT = false);

	// Apply owner ability-key toggle on client and refresh UI
	UFUNCTION(Client, Reliable)
	void Client_ApplyOwnerAbilityKeyToggle(AUnitBase* Unit, const FString& Key, bool bEnable);

	// Apply team-wide ability-key toggle on client and refresh UI
	UFUNCTION(Client, Reliable)
	void Client_ApplyTeamAbilityKeyToggle(int32 TeamId, const FString& Key, bool bEnable);

	UFUNCTION(Server, Reliable, BlueprintCallable,  Category = RTSUnitTemplate)
	void CorrectSetUnitMoveTargetForAbility(
		UObject* WorldContextObject,
		AUnitBase* Unit,
		const FVector& NewTargetLocation,
		float DesiredSpeed = 300.0f,
		float AcceptanceRadius = 50.0f,
		bool AttackT = false);

	UFUNCTION(Server, Reliable)
	void LoadUnitsMass(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnTransportUnitMass(FHitResult Hit_Pawn);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickPressedMass();


	/** Returns the world location of a unit, handling Actor vs. ISM-instance. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector GetUnitWorldLocation(const AUnitBase* Unit) const;
	
	/** Computes offsets for an N-unit grid formation centered at (0,0). */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<FVector> ComputeSlotOffsets(int32 NumUnits) const;
	/** Builds an N×N cost matrix of squared distances from unitsto slots. */
	TArray<TArray<float>> BuildCostMatrix(
		const TArray<FVector>& UnitPositions,
		const TArray<FVector>& SlotOffsets,
		const FVector& TargetCenter) const;
	
	/** Solves the assignment problem (Hungarian) on the given cost matrix. */
	TArray<int32> SolveHungarian(const TArray<TArray<float>>& Matrix) const;
	/** Determines if the formation needs to be recalculated. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool ShouldRecalculateFormation() const;
	
	/** Recalculates and stores unit formation offsets around TargetCenter. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RecalculateFormation(const FVector& TargetCenter);


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetHoldPositionOnSelectedUnits();

	UFUNCTION(Server, Reliable, BlueprintCallable,  Category = RTSUnitTemplate)
	void SetHoldPositionOnUnit(AUnitBase* Unit);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RunUnitsAndSetWaypointsMass(FHitResult Hit);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickPressedMass();

	// Server will process dragged abilities under cursor for selected units. If it does not early return,
	// it will notify the owning client to continue with selection logic.
	UFUNCTION(Server, Reliable)
	void Server_HandleAbilityUnderCursor(const TArray<AUnitBase*>& Units, const FHitResult& HitPawn, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound, bool bHasClientWorkAreaTransform, FTransform ClientWorkAreaTransform);

	// Owning client continues with selection under cursor when server indicates no early return.
	UFUNCTION(Client, Reliable)
	void Client_ContinueSelectionAfterAbility(const FHitResult& HitPawn);

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void LeftClickAttackMass(const TArray<AUnitBase*>& Units, const TArray<FVector>& Locations, bool AttackT, AActor* CursorHitActor = nullptr);

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void LeftClickAMoveUEPFMass(const TArray<AUnitBase*>& Units, const TArray<FVector>& Locations, bool AttackT);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickReleasedMass();

	// Minimap-specific commands
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickPressedMassMinimap(const FVector& GroundLocation);
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickPressedMassMinimapAttack(const FVector& GroundLocation);
	// Updates the Fog Mask using visible units
	UFUNCTION()
	void UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void UpdateMinimap(const TArray<FMassEntityHandle>& Entities);
	
	UPROPERTY()
	ASelectionCircleActor* SelectionCircleActor;
	
	UFUNCTION()
	void UpdateSelectionCircles();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetupPlayerMiniMap();

	UFUNCTION(Client, Reliable)
	void Client_ReceiveCooldown(int32 AbilityIndex, float RemainingTime);

	UFUNCTION(Server, Reliable)
	void Server_RequestCooldown(AUnitBase* Unit, int32 AbilityIndex, UGameplayAbilityBase* Ability);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RequestSetTeam(int32 NewTeamId);
	
	UFUNCTION(Server, Reliable)
	void Server_SetPendingTeam(int32 TeamId);

};
