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

	/**
	 * Loest die ausgewaehlten Einheiten von ihrem Wegpunkt, bevor ein Marschbefehl ergeht.
	 *
	 * IdleStateProcessor schreibt StoredLocation in JEDEM Idle-Takt auf den Wegpunkt zurueck,
	 * sobald FMassPatrolFragment::TargetWaypointLocation gesetzt ist - und die Regel direkt
	 * darunter laesst die Einheit zu StoredLocation zurueck laufen. Der Marschbefehl setzt
	 * StoredLocation zwar korrekt, wird aber im naechsten Takt ueberschrieben: die Einheit
	 * kehrte deshalb IMMER an ihre Ursprungsposition zurueck. Wer von Hand befehligt wird,
	 * verliert seinen Wegpunkt.
	 */
	void ClearWaypointForManualOrder(const TArray<AUnitBase*>& Units);

	// Tries to cancel active abilities for selected units. Returns true if any ability was canceled.
	bool TryCancelActiveAbilities();

public:

	/**
	 * Multi_SetMyTeamUnits selects the whole army once controllers are gathered. Turn this off
	 * where a pre-selected army is wrong - a recorded battle, a cinematic, a spectator view.
	 *
	 * LUX-ANPASSUNG (17.08.2026): Vorgabe jetzt AUS - Silvan: "Ich will dass beim Spielstart
	 * garkeine Einheit selektiert ist." Das Spiel beginnt damit mit leerer Auswahl.
	 * Das Schiessen bleibt davon unberuehrt: wird eine Faehigkeit ohne Auswahl ausgeloest,
	 * greift der vorhandene Rueckfall auf CameraUnitWithTag
	 * (ExtendedControllerBase::ActivateAbilitiesByIndex, ~Z. 1164) und selektiert die eigene
	 * Einheit in dem Moment selbst.
	 * Wer die alte Vorauswahl braucht, setzt den Haken am Controller-Blueprint wieder.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bSelectOwnUnitsOnMatchStart = false;
	AUnitBase* GetUnitFromHitResult(const FHitResult& Hit) const;

	virtual void BeginPlay() override;

	/** Drives the formation drag line: samples the cursor and polls for the right-mouse release. */
	virtual void Tick(float DeltaSeconds) override;

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
	
	// Zaehler fuer die Diagnose in CorrectSetUnitMoveTarget: wie oft ein Bewegungsziel ausserhalb des
	// Navigationsnetzes gesetzt wurde. Kein static - der Zaehler soll pro Controller und damit pro
	// PIE-Sitzung neu beginnen. Bewusst OHNE UPROPERTY und ausserhalb jeder Makro-Zeile: direkt hinter
	// einem UFUNCTION() haelt der Header-Parser die Variable fuer eine Funktion (Error C: "Found '='
	// when expecting '('").
	int32 ZielAusserhalbZaehler = 0;

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
	
	/** Computes offsets for an N-unit formation centered at (0,0), world-axis aligned. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<FVector> ComputeSlotOffsets(const TArray<AUnitBase*>& Units, float Spacing = -1.0f) const;

	/**
	 * Computes offsets for an N-unit formation centered at (0,0), for the currently selected
	 * GridFormationShape. Forward is the direction the group is heading in and orients the
	 * directional shapes (Circle/HalfCircle/Triangle); pass a zero vector or use a grid shape
	 * to get the historical world-axis-aligned layout.
	 *
	 * The returned array is indexed by SLOT, not by unit - RecalculateFormation matches units to
	 * slots afterwards. Units are expected to be sorted by descending capsule radius (as
	 * RecalculateFormation does), because the ring/row capacity math sizes each ring from the
	 * largest unit still unplaced.
	 *
	 * OutSlotCapacities, when supplied, receives the radius each slot was sized for, parallel to
	 * the returned offsets. BuildCostMatrix needs this: without it the solver is free to drop a
	 * large unit into a slot that was spaced for a small one, and they visually overlap.
	 */
	TArray<FVector> ComputeSlotOffsetsDirectional(const TArray<AUnitBase*>& Units, float Spacing, const FVector& Forward, TArray<float>* OutSlotCapacities = nullptr) const;

	/** Average direction the group will travel in, from the selection's centroid towards TargetCenter. Falls back to +X. */
	FVector ComputeApproachDirection(const TArray<AUnitBase*>& Units, const FVector& TargetCenter) const;

	/**
	 * Builds an N×N cost matrix of squared distances from units to slots, with size-compatibility penalties.
	 * SlotCapacities is the per-slot radius allowance from ComputeSlotOffsetsDirectional. When it is
	 * empty the capacities are re-derived from the row/column grid, which is only meaningful for the
	 * grid-based shapes.
	 */
	TArray<TArray<float>> BuildCostMatrix(
		const TArray<AUnitBase*>& Units,
		const TArray<FVector>& SlotOffsets,
		const FVector& TargetCenter,
		const TArray<float>& SlotCapacities = TArray<float>()) const;
	
	/** Solves the assignment problem (Hungarian) on the given cost matrix. */
	TArray<int32> SolveHungarian(const TArray<TArray<float>>& Matrix) const;
	/** Determines if the formation needs to be recalculated. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool ShouldRecalculateFormation() const;

	/** Invalidates the cached slot offsets so the next move order re-solves them (e.g. after a shape change). */
	virtual void ForceFormationRecalculation() override;
	
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

	// ------------------------------------------------------------------------------------------
	// Formation drag line: hold right mouse (move) or left mouse while attack-move is armed,
	// drag out a line, and on release the selection spreads evenly along it.
	//
	// All of this is client-local. Only the finished order crosses to the server, and it crosses
	// as final world locations through the existing batch RPCs - the server never learns that a
	// line was involved.
	// ------------------------------------------------------------------------------------------

	/** True while the player is holding the button down after a valid drag start. */
	UPROPERTY(BlueprintReadOnly, Category = "Formation Settings")
	bool bFormationLineDragActive = false;

	/**
	 * Hoehe der waagerechten Ebene, auf der die Formationslinie gezogen wird.
	 *
	 * Wird beim Beginn der Geste aus dem Startpunkt uebernommen. Ohne diese feste Ebene folgte die
	 * Linie der Gelaendehoehe unter dem Zeiger und verschob sich beim Ziehen ueber Huegel.
	 */
	float FormationLinePlaneZ = 0.f;

	/**
	 * AttackToggled as captured at press time. HandleAttackMovePressed clears AttackToggled at the
	 * end of the press, so by release time the original intent is gone unless we cache it here.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation Settings")
	bool bFormationLineDragIsAttackMove = false;

	/**
	 * Which button started the gesture, so the matching release finishes it. This is NOT fixed per
	 * feature: SwapAttackMove (toggled in the control widget) moves attack-move from the left
	 * button to the right one, and then BOTH the move drag and the attack-move drag live on the
	 * right button, told apart by AttackToggled.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation Settings")
	bool bFormationLineDragFromRightMouse = false;

	/**
	 * The units captured when the drag started. The order is issued to THESE, not to the live
	 * SelectedUnits: a box-select begun mid-gesture (perfectly possible while the other button is
	 * held) rewrites SelectedUnits, and without the snapshot the line would be handed to whatever
	 * the player happened to box afterwards.
	 */
	TArray<TWeakObjectPtr<AUnitBase>> FormationLineDragUnits;

	/** Hard ceiling on the min-spacing widening, so a huge selection cannot fling units off the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings", meta = (ClampMin = "100.0"))
	float FormationLineMaxLength = 20000.f;

	/**
	 * The raw cursor trail sampled during the drag. The formation follows this path, so the player
	 * can lay units out along a curve, not just a straight line. A straight drag simplifies back
	 * down to two points, so the straight case behaves exactly as before.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation Settings")
	TArray<FVector> FormationLinePath;

	/** Minimum cursor travel before another path sample is recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings", meta = (ClampMin = "10.0"))
	float FormationPathSampleDistance = 75.f;

	/**
	 * Douglas-Peucker tolerance. Deviations smaller than this are flattened away, so a hand that
	 * wobbles while dragging "straight" still produces a straight line, while a deliberate curve
	 * keeps its shape. Set to 0 to follow the raw trail exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings", meta = (ClampMin = "0.0"))
	float FormationPathSimplifyTolerance = 130.f;

	/** Bound on stored samples, so a very long drag cannot grow without limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings", meta = (ClampMin = "8", ClampMax = "2048"))
	int32 FormationPathMaxSamples = 512;

	UPROPERTY(BlueprintReadOnly, Category = "Formation Settings")
	FVector FormationLineStartWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Formation Settings")
	FVector FormationLineEndWorld = FVector::ZeroVector;

	/** Drag length in world units below which the gesture is treated as a plain click, not a line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings", meta = (ClampMin = "1.0"))
	float FormationLineDragThreshold = 150.f;

	/**
	 * When the drawn line is too short to hold the selection without the units interpenetrating,
	 * widen it symmetrically about its midpoint. Turn this off to honour the drawn endpoints
	 * exactly and let Mass avoidance sort out the crowding.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	bool bFormationLineEnforceMinSpacing = true;

	/** True when a drag is running AND long enough to count as a line. Drives the HUD preview. */
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	bool IsFormationLineDragValid() const;

	/**
	 * Whether a formation drag may start right now: a real group is selected and no other
	 * mouse-driven mode (work-area placement, ability targeting, unit dragging, waypoint queueing,
	 * tab overlay) owns the gesture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	bool CanStartFormationLineDrag() const;

	/** Latches the drag origin. Call only after CanStartFormationLineDrag(). */
	void BeginFormationLineDrag(const FVector& StartWorld, bool bAttackMove, bool bFromRightMouse);

	/** Called every frame from Tick while the button is held. */
	void UpdateFormationLineDrag(const FVector& CurrentWorld);

	/** Drops the gesture without issuing anything (menu opened, selection lost, controls blocked). */
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	void CancelFormationLineDrag();

	/** Can this unit actually be sent to a line slot? Buildings, dead and immobile units cannot. */
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	bool IsUnitEligibleForFormationLine(const AUnitBase* Unit) const;

	/**
	 * The polyline the order will ACTUALLY use for NumUnits: the sampled cursor trail, simplified,
	 * and extended along its end tangents when it is too short to hold them all without overlap
	 * (clamped to FormationLineMaxLength).
	 */
	void GetEffectiveFormationPath(int32 NumUnits, float MaxUnitRadius, TArray<FVector>& OutPath) const;

	/** Evenly spaced world points BY ARC LENGTH along Path. Returns exactly NumPoints entries. */
	static TArray<FVector> DistributeAlongPath(const TArray<FVector>& Path, int32 NumPoints);

	/**
	 * The single source of truth for both the HUD preview and the issued order: the eligible units
	 * in slot order, the polyline to draw, and one world slot per unit. Computing both from one
	 * place is what keeps the preview from promising something the release does not deliver.
	 */
	bool BuildFormationLineOrder(TArray<AUnitBase*>& OutUnits, TArray<FVector>& OutPath, TArray<FVector>& OutSlots) const;

	/**
	 * Evenly spaced world points along the current path, ordered start to end. Honours
	 * bFormationLineEnforceMinSpacing. Returns exactly Units.Num() entries, or empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	TArray<FVector> ComputeFormationLinePoints(const TArray<AUnitBase*>& Units) const;

	/**
	 * Ends the gesture on release of bFromRightMouse's button. No-ops when the released button is
	 * not the one that started the drag. If the drag was long enough, distributes the selection
	 * along the line and returns true; otherwise clears the state and returns false, leaving the
	 * order that was already issued at press time in place.
	 */
	bool FinishFormationLineDrag(bool bFromRightMouse);

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
