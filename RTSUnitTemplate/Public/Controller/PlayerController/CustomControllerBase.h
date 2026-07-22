// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"


#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassCommandBuffer.h"
#include "MassNavigationFragments.h"
#include "MassMovementFragments.h"
#include "Engine/World.h"        // Include for UWorld, GEngine
#include "Engine/Engine.h"       // Include for GEngine
#include "Engine/EngineTypes.h"   // For FHitResult in UFUNCTION params
#include "TimerManager.h"  // For FTimerHandle

class USoundBase;
class AUnitBase;
class AMassUnitBase;
class AMinimapActor;

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

	FTimerHandle MainHUDRetryTimerHandle;
	int32 MainHUDRetryCount = 0;

	// Helpers for follow-target deferral
	bool IsFollowCommandReady(const TArray<AUnitBase*>& Units);
	void ScheduleFollowRetry(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT, int32 MaxAttempts = 8, float DelaySeconds = 0.5f);
	void Retry_Server_SetUnitsFollowTarget();
	void ExecuteFollowCommand(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT);
	void ApplyTransportTags(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget);

	// Handles follow command on right-click. Returns true if a follow action was issued (and should early return)
	bool TryHandleFollowOnRightClick(const FHitResult& HitPawn);

	// Tries to cancel active abilities for selected units. Returns true if any ability was canceled.
	bool TryCancelActiveAbilities();

public:
	AUnitBase* GetUnitFromHitResult(const FHitResult& Hit) const;

	virtual void BeginPlay() override;

	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetMyTeamUnits(const TArray<AActor*>& AllUnits);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float VisibilityUpdateInterval = 0.05f;

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

	// Single source of truth for nav-validated batch move targets. Given the per-unit raw targets,
	// returns targets where any point on a dirty (UNavArea_Obstacle) or off-navmesh area has been
	// snapped to the nearest valid point via ValidateAndAdjustGridLocation (whole-grid shift first,
	// per-point snap as a fallback). Returns the input unchanged when every point is already valid.
	// Called on the commanding client (right-click move) BEFORE prediction + RPC, and on the server,
	// so server and all clients use identical destinations (fixes client units appearing stuck while
	// the server moved them, when a formation slot landed off-nav/dirty).
	TArray<FVector> AdjustBatchTargetsForNav(const TArray<AUnitBase*>& Units, const TArray<FVector>& InTargets);

	// Batched version to reduce per-unit RPC spamming when issuing group move orders
	// Now multicast so that all clients receive the movement updates, but invoked by a server wrapper
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void Batch_CorrectSetUnitMoveTargets(
		UObject* WorldContextObject,
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& NewTargetLocations,
		const TArray<float>& DesiredSpeeds,
		const TArray<float>& AcceptanceRadii,
		bool AttackT = false,
		bool bResetHoldPosition = true,
		bool bResetFollowTarget = true);

	// Server wrapper to validate and then trigger the multicast from the authority.
	// bOriginatorPredictsLocally: set true when the calling client already applied local prediction
	// (e.g. right-click move) -> the server skips the Client_Predict round-trip back to that originator
	// to avoid double-applying. Other callers leave it false and keep receiving the round-trip.
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_Batch_CorrectSetUnitMoveTargets(
		UObject* WorldContextObject,
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& NewTargetLocations,
		const TArray<float>& DesiredSpeeds,
		const TArray<float>& AcceptanceRadii,
		bool AttackT = false,
		bool bResetHoldPosition = true,
		bool bResetFollowTarget = true,
		bool bOriginatorPredictsLocally = false);

	// Client-side prediction: apply Run tag and local MoveTarget updates on each client.
	// Units are referenced by replicated UnitIndex (not actor pointers): object refs in RPCs null
	// out when their NetGUID isn't mapped at receive time (common under combat relevance churn),
	// whereas an int32 always serializes and the client resolves it locally via the binding cache.
	UFUNCTION(Client, Reliable)
	void Client_Predict_Batch_CorrectSetUnitMoveTargets(
		UObject* WorldContextObject,
		const TArray<int32>& UnitIndices,
		const TArray<FVector>& NewTargetLocations,
		const TArray<float>& DesiredSpeeds,
		const TArray<float>& AcceptanceRadii,
		bool AttackT = false,
		bool bResetHoldPosition = true,
		bool bResetFollowTarget = true);

	// Applies one unit's local move prediction (Run tag, MoveTarget/Pred fragment, state-tag cleanup).
	// Shared by the Client_Predict RPC handler (other clients, resolved via cache) and by the commanding
	// client's immediate local prediction (valid local refs, no server round-trip). The caller is
	// responsible for flushing deferred Mass commands afterwards (so a batch flushes once).
	void ApplyMovePredictionToUnit(
		FMassEntityManager& EntityManager,
		UWorld* World,
		AUnitBase* Unit,
		const FVector& NewTargetLocation,
		float DesiredSpeed,
		float AcceptanceRadius,
		bool AttackT,
		bool bResetHoldPosition,
		bool bResetFollowTarget);

	// Batch initialization of units without formation logic, projecting each to its own position on NavMesh.
	void Batch_KickUnits(const TArray<AUnitBase*>& Units);

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
	virtual void RightClickPressedMass();


	/** Returns the world location of a unit, handling Actor vs. ISM-instance. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector GetUnitWorldLocation(const AUnitBase* Unit) const;
	
	/** Computes offsets for an N-unit grid formation centered at (0,0). */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<FVector> ComputeSlotOffsets(const TArray<AUnitBase*>& Units, float Spacing = -1.0f) const;
	/** Builds an N×N cost matrix of squared distances from units to slots, with size-compatibility penalties. */
	TArray<TArray<float>> BuildCostMatrix(
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& SlotOffsets,
		const FVector& TargetCenter) const;
	
	/** Solves the assignment problem (Hungarian) on the given cost matrix. */
	TArray<int32> SolveHungarian(const TArray<TArray<float>>& Matrix) const;
	/** Determines if the formation needs to be recalculated. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool ShouldRecalculateFormation() const;
	
	/** Recalculates and stores unit formation offsets around TargetCenter. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RecalculateFormation(const FVector& TargetCenter, float Spacing = -1.0f);

	/** Validates and adjusts a target location and formation offsets to fit on the NavMesh. */
	bool ValidateAndAdjustGridLocation(const TArray<AUnitBase*>& Units, FVector& InOutLocation, TArray<FVector>& OutOffsets, float& OutSpacing);

	/** Returns true if the location is within any recently marked dirty area (obstacle). */

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
	void Server_HandleAbilityUnderCursor(const TArray<AUnitBase*>& Units, const FHitResult& HitPawn, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound, bool bHasClientWorkAreaTransform, FTransform ClientWorkAreaTransform, int32 InAbilityIndex);

	// Owning client continues with selection under cursor when server indicates no early return.
	UFUNCTION(Client, Reliable)
	void Client_ContinueSelectionAfterAbility(const FHitResult& HitPawn, bool bFromCooldown = false, bool bResetFlagOnly = false);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool SwapAttackMove = false;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleAttackMovePressed();

	void ShowFriendlyHealthbars();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float LastHealthBarPingTime = -100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<TSubclassOf<class UUserWidget>> MainHUDs;

	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
	class UUserWidget* MainHUDInstance;

	UFUNCTION(Client, Reliable)
	void Client_InitializeMainHUD();

	UFUNCTION(BlueprintCallable, Category = "Mass")
	void Batch_RemoveRotateToMouseTag();
	
	void Retry_InitializeMainHUD();

protected:
	UPROPERTY()
	AMinimapActor* CachedMinimapActor = nullptr;

	bool bStopMinimapSearch = false;
	float MinimapSearchEndTime = 0.0f;
	
	
	/** The extent used when projecting a point to the NavMesh to validate move commands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Navigation")
	FVector NavMeshProjectionExtent = FVector(50.f, 50.f, 250.f);

private:
	bool bDeselectOnNextClick = false;
};
