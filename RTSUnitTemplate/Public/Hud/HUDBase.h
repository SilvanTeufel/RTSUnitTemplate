// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "HUDBase.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class ABuildingBase;

UENUM()
enum ESelectionIndicatorStyle
{
	Circle,
	RotatingPartialCircle,
	Octagon,
	RotatingOctagon
};

UENUM()
enum EHealthBarStyle
{
	Stacked,
	SemiCircle,
	SideBrackets
};

USTRUCT(BlueprintType)
struct FHealthBarSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	TEnumAsByte<EHealthBarStyle> Style = Stacked;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float Thickness = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0", ClampMax = "256"))
	int32 Segments = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float SegmentSpace = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RadiusMultiplier = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MinScreenSize = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxScreenSize = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RotationOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bFaceCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bShowOutline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor OutlineColor = FColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float OutlineThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor HealthColor = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor ShieldColor = FColor::Cyan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor ManaColor = FColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor BackgroundColor = FColor(0, 0, 0, 150);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Level")
	bool bShowLevel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Level")
	FColor LevelColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Level")
	FVector2D LevelOffset = FVector2D(0.f, -25.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Level")
	float LevelTextScale = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float SegmentRefillThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float BarPadding = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bShowHealthbarOnSelected = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bDisableManaBar = false;
};

USTRUCT(BlueprintType)
struct FSelectionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	FColor Color = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	float Thickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	float SizeMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	TEnumAsByte<ESelectionIndicatorStyle> Style = Circle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	bool bEnableOcclusion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	float RotatingSpeed = 120.0f;

	/** Optional UI-domain material for the selection indicator. When set, the SAME shape as Style (Circle,
	 *  Octagon, RotatingPartialCircle, ...) is emitted as a material-textured band that follows the full
	 *  camera-angle occlusion fade + rotation + Thickness -- the material only skins the line. Tinted by
	 *  Color (occlusion fade in vertex alpha). Leave null for the classic flat-line shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	TObjectPtr<UMaterialInterface> SelectionMaterial = nullptr;
};

UENUM(BlueprintType)
enum ERectangleBorderType
{
	Line,
	Dashed,
	Dotted,
	DashDotted
};

// ---- Ribbon (material) line rendering --------------------------------------
UENUM(BlueprintType)
enum class ERibbonTilingMode : uint8
{
	ScreenSpace,   // U accumulates along projected screen length  (zoom-stable tile size on screen)
	WorldSpace     // U accumulates along world length            (tiles anchored to the ground)
};

USTRUCT(BlueprintType)
struct FSelectionRectangleSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	FLinearColor FillColor = FLinearColor(0, 1, 0, .15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	FLinearColor BorderColor = FLinearColor(0, 1, 0, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection", meta = (ClampMin = "0.0"))
	float BorderThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	TEnumAsByte<ERectangleBorderType> BorderType = Line;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection", meta = (ClampMin = "0.0"))
	float DashLength = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection", meta = (ClampMin = "0.0"))
	float GapLength = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	bool bDrawBorder = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	bool bDrawFill = true;
};

USTRUCT()
struct FClickIndicator
{
	GENERATED_BODY()

	UPROPERTY() FVector Location   = FVector::ZeroVector;
	UPROPERTY() FColor  Color      = FColor::White;
	UPROPERTY() float   StartTime  = 0.f;   // NEW — needed to derive 0..1 progress
	UPROPERTY() float   ExpiryTime = 0.f;
	UPROPERTY() float   Radius     = 0.f;

	// NEW — per-indicator MID; kept alive because it lives in a UPROPERTY array.
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
};

UCLASS()
class RTSUNITTEMPLATE_API AHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	void AddClickIndicator(FVector Location, FColor Color, float LifeTime = -1.f, float Radius = -1.f, UMaterialInterface* MaterialOverride = nullptr);

private:
	UPROPERTY(Transient)
	TArray<FClickIndicator> ClickIndicators;

	/**
	 * Zuletzt erzeugte Klick-Materialinstanz, zur Wiederverwendung innerhalb EINES Klicks.
	 *
	 * WOFUER: AddClickIndicator legte je Indikator eine eigene UMaterialInstanceDynamic an. Ein
	 * Rechtsklick erzeugt aber einen Indikator JE EINHEIT - bei 510 ausgewaehlten Einheiten also
	 * 510 Materialinstanzen in einem einzigen Bild.
	 *
	 * Gemessen am 17.09.2026 auf LevelSix: ein Rechtsklick kostete 202-276 ms. Davon entfielen
	 * nur rund 21 ms auf die Bodenspuren (eine einzelne Spur: 0,042 ms) - der weitaus groesste
	 * Teil ging fuer diese Materialinstanzen drauf.
	 *
	 * WARUM TEILEN ERLAUBT IST: die Instanz traegt nur Progress, Radius und Farbe. Alle
	 * Indikatoren desselben Klicks haben dieselbe Startzeit, denselben Radius und dieselbe Farbe,
	 * also auch denselben Progress in jedem Bild. Das Ergebnis ist Pixel fuer Pixel dasselbe.
	 * Sobald sich einer der drei Werte unterscheidet, entsteht wieder eine neue Instanz.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LastClickMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LastClickMaterial = nullptr;

	FColor LastClickColor = FColor::Transparent;
	float LastClickRadius = -1.f;
	float LastClickStartTime = -1.f;
	void DrawProjectedCircle(const FVector& Location, float Radius, FColor Color, float Thickness = -1.f, int32 InSegments = -1, bool bDisableSizeCulling = false);
	void DrawMaterialDisc(const FClickIndicator& Indicator);
	
	void DrawSelectionIndicator(class AUnitBase* Unit, const FVector& Location, float RadiusX, float RadiusY, const FRotator& Rotation, const FSelectionSettings& Settings, bool bDisableOcclusionOverride = false, int32 InSegments = -1);
	void DrawAllSelectedUnitsIndicators();
	void DrawAllHealthBars();
	void DrawStackedHealthBar(AUnitBase* Unit, const FVector& BaseLoc, const FVector2D& ScreenPos, float WorldRadius, const FHealthBarSettings& Settings, const FVector& RightV);
	void DrawLevelText(AUnitBase* Unit, const FVector2D& ScreenPos, const FHealthBarSettings& Settings);
	void DrawSemiCircleHealthBar(AUnitBase* Unit, const FVector& BaseLoc, const FVector2D& ScreenPos, float RadiusX, float RadiusY, bool bIsFlying, const FHealthBarSettings& Settings, const FVector& RightV, const FVector& UpV);
	void DrawSideBracketsHealthBar(AUnitBase* Unit, const FVector& BaseLoc, const FVector2D& ScreenPos, float WorldRadius, float WorldWidthRadius, const FHealthBarSettings& Settings, const FVector& RightV, const FVector& UpV);
	float GetHysteresisPct(float ActualPct, float& DisplayedPct, const FHealthBarSettings& Settings);

protected:
	void HandleSelectionRectangle();
	void DrawDashedLine2D(const FVector2D& Start, const FVector2D& End, float DashLen, float GapLen, FLinearColor Color, float Thickness);
	void DrawDashDottedLine2D(const FVector2D& Start, const FVector2D& End, float DashLen, float GapLen, FLinearColor Color, float Thickness);

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	FSelectionRectangleSettings SelectionRectSettings;
	
	UFUNCTION(BlueprintCallable, Category = "RTS|HUD")
	void RegisterUnit(class AUnitBase* Unit);

	UFUNCTION(BlueprintCallable, Category = "RTS|HUD")
	void UnregisterUnit(class AUnitBase* Unit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	FSelectionSettings BuildingSelectionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	FSelectionSettings FlyingSelectionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	FSelectionSettings GroundSelectionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	bool bEnableStandardSelection = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	bool bEnableHealthBars = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	bool bShowAllHealthBarsPermanent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	UFont* LevelFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings BuildingHealthBarSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings FlyingHealthBarSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings GroundHealthBarSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings ConstructionHealthBarSettings;

	// --- Resource worker-count display -------------------------------------------------------------
	// Draws "N/Max" (Canvas world-text, like the level text) over each resource WorkArea. Nothing is
	// drawn when N == 0 or MaxWorkerCount <= 0. No per-node widgets -> cheap for an RTS.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Resources")
	bool bShowResourceWorkerCounts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Resources")
	FColor ResourceCountColor = FColor(255, 235, 140, 255);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Resources")
	float ResourceCountTextScale = 1.0f;

	// World-space Z offset (above the node origin) at which the count is drawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Resources")
	float ResourceCountHeightOffset = 120.f;

	// Draws the N/Max worker count over all resource WorkAreas (called from DrawHUD).
	void DrawAllResourceCounts();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float ClickIndicatorRadius = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float ClickIndicatorThickness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float ClickIndicatorLifeTime = 1.5f;

	/** Optional material for the move-click indicator. When set, the indicator renders as an
	 *  animated ground-projected material quad instead of the solid ring. Leave null for the ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TObjectPtr<UMaterialInterface> ClickIndicatorMaterial = nullptr;

	/** Scalar param driven 0..1 over the indicator lifetime (ripple phase). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FName ClickIndicatorProgressParamName = FName("Progress");

	/** Scalar param set to the world radius (optional; lets the material size its ring). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FName ClickIndicatorRadiusParamName = FName("Radius");

	/** Vector param set to the click colour (optional tint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	FName ClickIndicatorColorParamName = FName("Color");

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	virtual void DrawHUD();

	// ------------------------------------------------------------------------------------------
	// Formation drag line preview.
	//
	// Stored in WORLD space, not screen space: the controller already has the ground point from
	// its cursor trace, the release path needs those exact world points anyway, and projecting per
	// frame gives correct terrain foreshortening for free.
	//
	// Unlike ExtensionPreviewLine this does NOT self-clear after drawing - the controller sets it
	// on press, updates it per tick and clears it on release, so a frame where the controller does
	// not push would otherwise make the line flicker.
	// ------------------------------------------------------------------------------------------

	/**
	 * Sets/refreshes the preview. Path is the polyline to draw (2+ points; a straight drag gives
	 * exactly 2), SlotPositions are the exact world positions the units will be sent to. Both come
	 * from the controller's single BuildFormationLineOrder, so the preview cannot drift from the
	 * order that gets issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Formation")
	void UpdateFormationPath(const TArray<FVector>& Path, const TArray<FVector>& SlotPositions);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Formation")
	void ClearFormationLine();

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Formation")
	bool bFormationLineActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Formation")
	TArray<FVector> FormationPathPoints;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Formation")
	TArray<FVector> FormationSlotPositions;

	/** Bound on drawn polyline segments, mirroring the MaxSegments discipline elsewhere in this file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation", meta = (ClampMin = "2", ClampMax = "1024"))
	int32 FormationPathMaxSegments = 256;

	/**
	 * Zeichnet die gestrichelte Linie entlang der gezogenen Formation.
	 *
	 * Aus, weil die Punkte je Einheit bereits zeigen, wohin marschiert wird - die Linie war nur
	 * zusaetzliches Gekritzel. Die Punkte bleiben davon unberuehrt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bDrawFormationPathLine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation")
	FColor FormationLineColor = FColor(60, 220, 90, 255);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation", meta = (ClampMin = "1.0"))
	float FormationLineDashLen = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation", meta = (ClampMin = "0.0"))
	float FormationLineGapLen = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation", meta = (ClampMin = "0.1"))
	float FormationLineThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation", meta = (ClampMin = "1.0"))
	float FormationMarkerRadius = 26.f;

	/** Hard cap on drawn slot markers. SelectedUnits is Blueprint-writable, so this must be clamped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation", meta = (ClampMin = "1", ClampMax = "256"))
	int32 FormationMaxMarkers = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Formation")
	float FormationLineZOffset = 12.f;

protected:
	/** Draws the line plus evenly spaced slot markers. Guards its own Canvas/PC access. */
	void DrawFormationLinePreview();

public:

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SelectISMUnitsInRectangle(const FVector2D& RectMin, const FVector2D& RectMax);
	
	void Tick(float DeltaSeconds);
	void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	bool bSelectFullSquad = false;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SelectUnitsFromSameSquad(AUnitBase* SelectedUnit);

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	TArray <AUnitBase*> FriendlyUnits;

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	TArray <AUnitBase*> EnemyUnitBases;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void AddUnitsToArray();
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "bSelectFriendly", Keywords = "TopDownRTSTemplate bSelectFriendly"), Category = "TopDownRTSTemplate")
	bool bSelectFriendly = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "InitialPoint", Keywords = "RTSUnitTemplate InitialPoint"), Category = "RTSUnitTemplate")
	FVector2D InitialPoint;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CurrentPoint", Keywords = "RTSUnitTemplate CurrentPoint"), Category = "RTSUnitTemplate")
	FVector2D CurrentPoint;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IPoint", Keywords = "RTSUnitTemplate IPoint"), Category = "RTSUnitTemplate")
	FVector IPoint = FVector(0.f,0.f, 0.f);

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CPoint", Keywords = "RTSUnitTemplate CPoint"), Category = "RTSUnitTemplate")
	FVector CPoint = FVector(0.f,0.f, 0.f);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "RectangleScaleSelectionFactor", Keywords = "RTSUnitTemplate RectangleScaleSelectionFactor"), Category = "RTSUnitTemplate")
	float RectangleScaleSelectionFactor = 0.9f;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMousePos2D", Keywords = "RTSUnitTemplate GetMousePos2D"), Category = "RTSUnitTemplate")
	FVector2D GetMousePos2D();

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SelectedEnemyUnits", Keywords = "RTSUnitTemplate SelectedEnemyUnits"), Category = "RTSUnitTemplate")
	TArray <AUnitBase*> SelectedUnits;

	UPROPERTY()
	TSet <AUnitBase*> SelectedUnitsSet;

	int32 LastFrameVisibleCount = 0;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsSpeakingUnitClose", Keywords = "RTSUnitTemplate IsSpeakingUnitClose"), Category = "TopDownRTSTemplate")
	void IsSpeakingUnitClose(TArray <AUnitBase*> Units, TArray <ASpeakingUnit*> SpeakUnits);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CharacterIsUnSelectable", Keywords = "RTSUnitTemplate CharacterIsUnSelectable"), Category = "RTSUnitTemplate")
	bool CharacterIsUnSelectable = true;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveUnitsThroughWayPoints", Keywords = "RTSUnitTemplate MoveUnitsThroughWayPoints"), Category = "TopDownRTSTemplate")
	void MoveUnitsThroughWayPoints(TArray <AUnitBase*> Units);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "PatrolUnitsThroughWayPoints", Keywords = "RTSUnitTemplate PatrolUnitsThroughWayPoints"), Category = "TopDownRTSTemplate")
	void PatrolUnitsThroughWayPoints(TArray <AUnitBase*> Units);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetUnitSelected", Keywords = "RTSUnitTemplate SetUnitSelected"), Category = "TopDownRTSTemplate")
	void SetUnitSelected(AUnitBase* Unit, bool bIsAi = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DeselectAllUnits", Keywords = "RTSUnitTemplate DeselectAllUnits"), Category = "TopDownRTSTemplate")
	void DeselectAllUnits();

	// ================================================================================================
	// LUX-ANPASSUNG (17.08.2026) - jeder Spieler soll nur SEINE CameraUnit selektieren koennen.
	// Die Assignment macht bereits der GameMode ueber "Character.CameraUnit.<Spielerindex>"
	// (RTSGameModeBase, SetCameraUnitWithTag) - Spieler 1 bekommt .0, Spieler 2 bekommt .1 usw.
	// Diese Pruefung setzt das in der Auswahl durch: Einheiten mit einem CameraUnit-Tag sind nur
	// fuer den Spieler waehlbar, dem sie zugewiesen wurden. Alle anderen Einheiten bleiben
	// unveraendert selektierbar.
	// ================================================================================================
	UFUNCTION(BlueprintCallable, Category = "TopDownRTSTemplate")
	bool IsForeignCameraUnit(const AUnitBase* Unit) const;

	bool IsActorInsideRec(FVector InPoint, FVector CuPoint, FVector ALocation);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float SelectionLineThickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float SelectionLineDashLen = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float SelectionLineGapLen = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WaypointLineHeightOffset = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WaypointCircleRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WaypointCircleThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	FColor WaypointLineColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float ExtensionLineThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float ExtensionLineDashLen = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float ExtensionLineGapLen = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WPLineZOffset = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	int32 WPLineMaxIterations = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WPLineCollisionZOffset = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WPLineDashLen = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WPLineGapLen = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	FColor WPLineColor = FColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float WPLineThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	FColor UnitWPLineColorAttackMove = FColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	FColor UnitWPLineColorMove = FColor::Green;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float UnitWPLineDashLen = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float UnitWPLineGapLen = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float UnitWPLineThickness = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	float UnitWPLineZOffset = 10.f;

	// Fallback local-space origin offset for building rally lines, used when a building leaves its
	// WaypointLineOriginOffset at zero AND has no resolving WaypointLineOriginSocket. Rotated by the
	// building's actor rotation. Zero = pivot (legacy).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection")
	FVector WPLineDefaultOriginOffset = FVector::ZeroVector;

	// ---- Ribbon (material) line rendering --------------------------------------
	// Shared ribbon controls
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	ERibbonTilingMode RibbonTilingMode = ERibbonTilingMode::ScreenSpace;

	// U density. ScreenSpace: screen pixels per one 0..1 U repeat. WorldSpace: world units per repeat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon", meta = (ClampMin = "1.0"))
	float RibbonMaterialTiling = 64.f;

	// Hard cap on polyline points fed to a single strip (OOM backstop).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon", meta = (ClampMin = "2", ClampMax = "1024"))
	int32 RibbonMaxPoints = 256;

	// Per-line-type material slots. When null -> existing dashed/solid fallback is used (backward compatible).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	TObjectPtr<UMaterialInterface> WPLineMaterial = nullptr;         // building rally links

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon", meta = (ClampMin = "0.5"))
	float WPLineRibbonWidth = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	TObjectPtr<UMaterialInterface> UnitWPLineMaterial = nullptr;     // unit movement paths

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon", meta = (ClampMin = "0.5"))
	float UnitWPLineRibbonWidth = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	TObjectPtr<UMaterialInterface> ExtensionLineMaterial = nullptr;  // extension preview

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon", meta = (ClampMin = "0.5"))
	float ExtensionLineRibbonWidth = 10.f;

	/** When set, the EnergyWall placement preview is drawn as a translucent vertical quad (a "wall")
	 *  spanning the two pillars instead of lines. Tinted by the preview color (green = valid / red = blocked);
	 *  put the flicker in the material (Sin(Time) on Opacity). Material Domain must be User Interface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	TObjectPtr<UMaterialInterface> ExtensionWallMaterial = nullptr;

	/** Wall height in world units. If <= 0, uses ExtensionPreviewLine.HeightOffset * 2 (matches the old top edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	float ExtensionWallHeight = 300.f;

	/** Base vertex alpha of the wall quad (the material may modulate it further). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExtensionWallOpacity = 0.4f;

	/** How far (world units) the wall base is dropped below the pillar pivots so the wall starts at the
	 *  ground instead of the pivot mid-height. Increase if it still floats; decrease if it sinks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Selection|Ribbon")
	float ExtensionWallBaseDrop = 100.f;

	// Draws a translucent vertical quad ("wall") between Start and End up to Height, via Material, tinted Color.
	void DrawMaterialWall(const FVector& Start, const FVector& End, float Height, class UMaterialInterface* Material, const FLinearColor& Color);

	// ---- Bar (health/shield/mana/movement) material rendering ----------------------------------------
	/** Single shared UI-domain material for ALL stat bars (health/shield/mana/movement) across all unit
	 *  types. When set, each bar's FILL is drawn via the material (fill fraction in vertex alpha, tint in
	 *  vertex rgb) instead of flat tiles -> one shared material batches into ~1 draw call. ALL FHealthBarSettings
	 *  are preserved: Segments still render as discrete material tiles, Outline/Background/Level are untouched.
	 *  Leave null for the classic flat-tile fill. Applies to Stacked, SideBrackets and SemiCircle styles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Bars")
	TObjectPtr<UMaterialInterface> BarMaterial = nullptr;

	// Draws one stat-bar FILL as a material quad. FillPct -> vertex alpha, FillColor -> vertex rgb;
	// UV.x = position along the fill axis (0..1), UV.y across the bar. bVertical fills bottom->top.
	void DrawMaterialBar(const FVector2D& Pos, const FVector2D& Size, float FillPct, const FLinearColor& FillColor, bool bVertical);

	void DrawDashedLine3D(const FVector& Start, const FVector& End, float DashLen, float GapLen, FColor Color, float Thickness, float ZOffset);

	// High-level: project a world polyline and draw it as one continuous material ribbon.
	void DrawRibbonLine3D(const TArray<FVector>& WorldPoints, UMaterialInterface* Material,
	                      const FLinearColor& Color, float WidthPx, float ZOffset);

	// High-level: screen-space polyline ribbon (selection rect / 2D preview).
	void DrawRibbonLine2D(const TArray<FVector2D>& ScreenPoints, UMaterialInterface* Material,
	                      const FLinearColor& Color, float WidthPx);

private:
	// Low-level strip emitter: pre-projected screen points + parallel accumulated-U array.
	void EmitRibbonStrip(const TArray<FVector2D>& Pts, const TArray<float>& CumU,
	                     UMaterialInterface* Material, const FLinearColor& Color, float WidthPx);

	// Reused across frames to avoid per-call heap churn.
	TArray<FCanvasUVTri> RibbonScratch;

	struct FExtensionPreviewLine
	{
		FVector Start;
		FVector End;
		FColor Color;
		float HeightOffset;
		bool bIsActive;

		FExtensionPreviewLine() : Start(0.f), End(0.f), Color(FColor::White), HeightOffset(0.f), bIsActive(false) {}
	};

	FExtensionPreviewLine ExtensionPreviewLine;

public:
	void SetExtensionPreviewLine(FVector Start, FVector End, FColor Color, float HeightOffset);

private:
	void DrawSelectedBuildingWaypointLinks();
	void DrawSelectedUnitsMovementLines();

	// Resolve the world-space START of a building's rally/waypoint line.
	FVector ResolveBuildingWaypointOrigin(const AUnitBase* Unit, const ABuildingBase* Config, bool bAllowSocket) const;
};
