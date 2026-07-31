// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hud/PathProviderHUD.h"
#include "Characters/Camera/CameraBase.h"
#include "Core/UnitData.h"
#include "GameFramework/PlayerController.h"
#include "Actors/EffectArea.h"
#include "Actors/UnitSpawnPlatform.h"
#include "Kismet/GameplayStatics.h"
#include "GameModes/RTSGameModeBase.h"

class AWorkArea;
class AActor;
class UStaticMeshComponent;
class USoundBase;
class AUnitBase;
class AAbilityIndicator;
class UGameplayAbilityBase;
class URTSMouseCursorWidget;
class UMaterialInterface;
class UTexture2D;

#include "ControllerBase.generated.h"

/**
 * 
 */

// NOTE: append new shapes at the END only. The enumerator ORDER is what gets serialized into
// Blueprint class-default overrides, so re-ordering silently rewrites every saved selection.
UENUM(BlueprintType)
enum class EGridShape : uint8
{
	// Axis-aligned rectangular grid, ceil(sqrt(N)) columns. The historical default look.
	Square			UMETA(DisplayName = "Rectangle"),
	// Same grid, but odd rows are shifted half a column sideways (brick pattern).
	Staggered		UMETA(DisplayName = "Rectangle (Staggered)"),
	// One single column.
	VerticalLine	UMETA(DisplayName = "Vertical Line"),
	// Concentric rings filling a disc, ring capacity derived from unit radii.
	Circle			UMETA(DisplayName = "Circle (multi-row)"),
	// Concentric arcs spanning 180 degrees, the bulge facing the move direction.
	HalfCircle		UMETA(DisplayName = "Half Circle (multi-row)"),
	// Wedge with rows of 1, 2, 3, ... units, apex facing the move direction.
	Triangle		UMETA(DisplayName = "Triangle / Wedge")
};

// Shapes whose slots are laid out on a regular row/column grid. Only for these does the
// per-slot size-compatibility penalty in BuildCostMatrix (which derives a slot's capacity
// from its grid row/column) describe anything real.
FORCEINLINE bool IsGridBasedFormationShape(EGridShape Shape)
{
	return Shape == EGridShape::Square || Shape == EGridShape::Staggered || Shape == EGridShape::VerticalLine;
}

// Shapes that are oriented along the direction the group is moving in, rather than
// being axis-aligned in world space.
FORCEINLINE bool IsDirectionalFormationShape(EGridShape Shape)
{
	return Shape == EGridShape::Circle || Shape == EGridShape::HalfCircle || Shape == EGridShape::Triangle;
}


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamIdChanged, int32, NewTeamId);

// Fires whenever the formation shape changes, from any source (C hotkey, picker widget, Blueprint).
// The picker subscribes to this instead of polling: UUserWidget::TickFrequency defaults to Auto,
// which switches ticking OFF for a widget with no Blueprint Tick event, so a NativeTick-based
// refresh would silently never run.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGridFormationShapeChanged, EGridShape, NewShape);

UCLASS()
class RTSUNITTEMPLATE_API AControllerBase : public APlayerController
{
	GENERATED_BODY()

public:
	AControllerBase();

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIsAi = false;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|Team")
	FOnTeamIdChanged OnTeamIdChanged;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	// ---- Configurable mouse cursor (Material and/or Icon) --------------------------------------
	// Pick a custom cursor in any BP controller's Class Defaults: set CursorMaterial (preferred,
	// needs a UI-domain material) OR CursorIcon (texture). Leave both null to keep the engine
	// crosshair. A material requires the software-widget path; a texture works either way.

	/** Software cursor widget class used to render the Material/Icon. Defaults to the built-in
	 *  URTSMouseCursorWidget so no companion asset is needed; point it at a WBP subclass to customize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cursor")
	TSubclassOf<URTSMouseCursorWidget> MouseCursorWidgetClass;

	/** UI-domain material drawn as the cursor. Takes precedence over CursorIcon. Requires the widget path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cursor")
	TObjectPtr<UMaterialInterface> CursorMaterial = nullptr;

	/** Texture drawn as the cursor when no CursorMaterial is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cursor")
	TObjectPtr<UTexture2D> CursorIcon = nullptr;

	/** Draw size (pixels) of the software cursor image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cursor")
	FVector2D CursorSize = FVector2D(32.f, 32.f);

	/** Cursor slot the widget replaces; kept in sync with DefaultMouseCursor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Cursor")
	TEnumAsByte<EMouseCursor::Type> MouseCursorType = EMouseCursor::Crosshairs;

	/** Cached software-cursor widget instance. */
	UPROPERTY(Transient)
	TObjectPtr<URTSMouseCursorWidget> MouseCursorWidgetInstance = nullptr;

	/** (Re)applies the configured cursor. Call again after changing the cursor properties at runtime. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Cursor")
	void ApplyCustomMouseCursor();

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AAbilityIndicator* CurrentDraggedAbilityIndicator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* WaypointSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* RunSound;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CameraUnitWithTag;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	float RunSoundDelayTime = 3.0f;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float LastRunSoundTime = 0.f;;
	// Timer handle for managing FPS display updates
	FTimerHandle FPSTimerHandle;

	FTimerHandle CheckSelectionTimerHandle;
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void InitCameraHUDGameMode();

	/** Client-callable request to enter read-only spectate. Routed to the authority GameMode's
	 *  EnterSpectate (base is a no-op; AExodusGameMode performs the pawn swap). Works from any client
	 *  (e.g. the WinLose widget's Spectator button) because the RPC executes server-side. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "RTSUnitTemplate|Spectator")
	void Server_EnterSpectate(bool bRevealAll);

	// Function called by timer to display FPS
	void DisplayUnitCount();
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void ToggleUnitCountDisplay(bool bEnable);
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	float GetSoundMultiplier() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float SoundMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	APathProviderHUD* HUDBase;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	ARTSGameModeBase* RTSGameMode;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	ACameraBase* CameraBase;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* ClickedActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	TSubclassOf<class AWaypoint> WaypointClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ShowUnitCount = false;
	
	virtual void Tick(float DeltaSeconds) override;
	
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShiftPressed", Keywords = "RTSUnitTemplate ShiftPressed"), Category = RTSUnitTemplate)
	void ShiftPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShiftReleased", Keywords = "RTSUnitTemplate ShiftReleased"), Category = RTSUnitTemplate)
	void ShiftReleased();

	// Client->server sync of the Shift state. IsShiftPressed replicates only server->client, so
	// input set on a remote client never reaches the server; this RPC mirrors it up so
	// server-authoritative reads of IsShiftPressed (waypoint queueing, shift-chained WorkArea
	// placement, keep-selection) work for remote clients too, not just the listen-server host.
	UFUNCTION(Server, Reliable)
	void Server_SetShiftPressed(bool bPressed);

	UFUNCTION(BlueprintCallable, Category = TopDownRTSTemplate)
	void SelectUnit(int Index);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickAMoveUEPF(AUnitBase* Unit, FVector Location);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickAMove(AUnitBase* Unit, FVector Location);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickAttack(AUnitBase* Unit, FVector Location);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void FireAbilityMouseHit(AUnitBase* Unit, const FHitResult& InHitResult);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickSelect();

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int CurrentUnitWidgetIndex = 0;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int GetHighestPriorityWidgetIndex();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetWidgets(int Index);

	// Removes a unit from the local selection (HUD + controller SelectedUnits) and keeps
	// CurrentUnitWidgetIndex consistent. Pure local UI bookkeeping, no replication — safe to
	// call on any machine (operates on this PC's own HUDBase).
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveUnitFromSelection(AUnitBase* Unit);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetRunLocation(AUnitBase* Unit, const FVector& DestinationLocation);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetUnitState_Replication(AUnitBase* Unit, int State);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetUnitState_Multi(AUnitBase* Unit, int State);
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsShiftPressed", Keywords = "RTSUnitTemplate IsShiftPressed"), Category = RTSUnitTemplate)
	bool UseUnrealEnginePathFinding = true;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UEPathfindingCornerOffset = 100.f;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetToggleUnitDetection(AUnitBase* Unit, bool State);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickRunShift(AUnitBase* Unit, FVector Location);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickRunUEPF(AUnitBase* Unit, FVector Location, bool CancelAbility);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickRunDijkstraPF(AUnitBase* Unit, FVector Location, int Counter);

	// Generalized to AUnitBase so it serves both finished buildings and construction sites
	// (AConstructionUnit). TeamId + NextWaypoint live on AUnitBase; assignment goes through
	// SetWaypoint() which registers AddAssignedUnit only for buildings (construction sites get a
	// raw assign, so they don't linger in the waypoint's AssignedUnits across the build handoff).
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWaypoint* CreateAWaypoint(FVector NewWPLocation, AUnitBase* OwnerUnit);
	void UnregisterWaypointFromBuilding(ABuildingBase* Building);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetBuildingWaypoint(FVector NewWPLocation, AUnitBase* Unit, AWaypoint* BuildingWaypoint, bool bPlaySound);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBuildingWaypoint(FVector NewWPLocation, AUnitBase* Unit, AWaypoint*& BuildingWaypoint, bool& PlayWaypointSound, bool& Success);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_SetBuildingWaypoint(FVector NewWPLocation, AUnitBase* Unit);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_Batch_SetBuildingWaypoints(const TArray<FVector>& NewWPLocations, const TArray<AUnitBase*>& Units);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	float GridSpacing = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	float GridCapsuleMultiplier = 1.5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	EGridShape GridFormationShape = EGridShape::Square;

	// The shapes the C key rotates through, in order. Any shape left out of this list stays
	// selectable in the dropdown but is skipped by the hotkey. Duplicates are allowed.
	// Kept in EGridShape order so the hotkey and the dropdown read the same way round.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	TArray<EGridShape> FormationCycleShapes = {
		EGridShape::Square,
		EGridShape::Staggered,
		EGridShape::VerticalLine,
		EGridShape::Circle,
		EGridShape::HalfCircle,
		EGridShape::Triangle
	};

	// Advances GridFormationShape to the next entry of FormationCycleShapes and forces the next
	// move order to rebuild its slots.
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	void CycleGridFormationShape();

	// Jumps straight to a shape (same force-rebuild semantics as CycleGridFormationShape).
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	void SetGridFormationShape(EGridShape NewShape);

	// Slots are solved on the commanding client and only final world locations reach the server,
	// so the shape does not need to replicate for the normal case. The exception is
	// AdjustBatchTargetsForNav: when a slot lands off-navmesh the SERVER re-solves the formation
	// with its own copy of these settings. Without this mirror it would rebuild a rectangle and
	// snap the unit out of whatever shape the player picked.
	UFUNCTION(Server, Reliable)
	void Server_SetGridFormationShape(EGridShape NewShape);

	// Human-readable name of the active shape, for on-screen feedback.
	UFUNCTION(BlueprintCallable, Category = "Formation Settings")
	FText GetGridFormationShapeText() const;

	UPROPERTY(BlueprintAssignable, Category = "Formation Settings")
	FOnGridFormationShapeChanged OnGridFormationShapeChanged;

	// Overridden by ACustomControllerBase to invalidate its cached per-unit slot offsets.
	virtual void ForceFormationRecalculation() {}

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 ComputeGridSize(int32 NumUnits) const;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector CalculateGridOffset(int32 Row, int32 Col) const;


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DrawCircleAtLocation(UWorld* World, const FVector& Location, FColor CircleColor);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector TraceRunLocation(FVector RunLocation, bool& HitNavModifier);

	bool IsLocationInDirtyArea(const FVector& Location) const;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RunUnitsAndSetWaypoints(FHitResult Hit);

	
	UFUNCTION(meta = (DisplayName = "SetRunLocationUseDijkstra", Keywords = "RTSUnitTemplate SetRunLocationUseDijkstra"), Category = RTSUnitTemplate)
	void SetRunLocationUseDijkstra(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i);

	UFUNCTION(meta = (DisplayName = "SetRunLocationUseDijkstra", Keywords = "RTSUnitTemplate SetRunLocationUseDijkstra"), Category = RTSUnitTemplate)
	void SetRunLocationUseDijkstraForAI(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpacePressed", Keywords = "RTSUnitTemplate SpacePressed"), Category = RTSUnitTemplate)
	void SpacePressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpaceReleased", Keywords = "RTSUnitTemplate SpaceReleased"), Category = RTSUnitTemplate)
	void SpaceReleased();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ToggleUnitDetection(AUnitBase* Unit);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "APressed", Keywords = "RTSUnitTemplate APressed"), Category = RTSUnitTemplate)
	void TPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "AReleased", Keywords = "RTSUnitTemplate AReleased"), Category = RTSUnitTemplate)
	void AReleased();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "JumpCamera", Keywords = "RTSUnitTemplate JumpCamera"), Category = RTSUnitTemplate)
	void JumpCamera();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "StrgPressed", Keywords = "RTSUnitTemplate StrgPressed"), Category = RTSUnitTemplate)
	void StrgPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "StrgReleased", Keywords = "RTSUnitTemplate StrgReleased"), Category = RTSUnitTemplate)
	void StrgReleased();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomIn", Keywords = "RTSUnitTemplate ZoomIn"), Category = RTSUnitTemplate)
	void ZoomIn();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomOut", Keywords = "RTSUnitTemplate ZoomOut"), Category = RTSUnitTemplate)
	void ZoomOut();

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AMissileRain> MissileRainClass;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnMissileRain(int TeamId, FVector Location);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AEffectArea> EffectAreaClass;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AEffectArea* SpawnEffectArea(int TeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, AUnitBase* ActorToLockOn = nullptr);
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsShiftPressed", Keywords = "RTSUnitTemplate IsShiftPressed"), Category = RTSUnitTemplate)
	bool IsShiftPressed = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AttackToggled", Keywords = "RTSUnitTemplate AttackToggled"), Category = RTSUnitTemplate)
	bool AttackToggled = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsStrgPressed", Keywords = "RTSUnitTemplate IsStrgPressed"), Category = RTSUnitTemplate)
	bool IsCtrlPressed = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "IsSpacePressed", Keywords = "RTSUnitTemplate IsSpacePressed"), Category = RTSUnitTemplate)
	bool IsSpacePressed = false;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AltIsPressed", Keywords = "RTSUnitTemplate AltIsPressed"), Category = RTSUnitTemplate)
	bool AltIsPressed = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "LeftClickisPressed", Keywords = "RTSUnitTemplate LeftClickisPressed"), Category = RTSUnitTemplate)
	bool LeftClickIsPressed = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "LockCameraToUnit", Keywords = "RTSUnitTemplate LockCameraToUnit"), Category = RTSUnitTemplate)
	bool LockCameraToUnit = false;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "AIsPressed", Keywords = "TopDownRTSCamLib AIsPressed"), Category = RTSUnitTemplate)
	int AIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "DIsPressed", Keywords = "TopDownRTSCamLib DIsPressed"), Category = RTSUnitTemplate)
	int DIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "WIsPressed", Keywords = "TopDownRTSCamLib WIsPressed"), Category = RTSUnitTemplate)
	int WIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "SIsPressed", Keywords = "TopDownRTSCamLib SIsPressed"), Category = RTSUnitTemplate)
	int SIsPressedState = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool MiddleMouseIsPressed = false;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AUnitBase*> SelectedUnits;
	
	UPROPERTY(ReplicatedUsing = OnRep_SelectableTeamId, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int SelectableTeamId = -1;

	UPROPERTY(ReplicatedUsing = OnRep_AlliedTeamsMask, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int64 AlliedTeamsMask = 0;

	UFUNCTION()
	void OnRep_AlliedTeamsMask();

	UFUNCTION()
	void OnRep_SelectableTeamId();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AWaypoint* DefaultWaypoint;
		
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Multi_SetControllerTeamId(int Id);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Multi_SetControllerDefaultWaypoint(AWaypoint* Waypoint);
	
	UPROPERTY(BlueprintReadWrite, Category = TopDownRTSTemplate)
	int SelectedUnitCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = TopDownRTSTemplate)
	float RelocateWaypointZOffset = 30.f;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void CancelCurrentAbility(AUnitBase* UnitBase);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void DeQueAbility(AUnitBase* UnitBase, int ButtonIndex);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveUnitToChase(AUnitBase* DetectingUnit, AActor* OtherActor);
};