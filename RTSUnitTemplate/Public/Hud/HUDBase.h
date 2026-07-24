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
	bool bShowAllHealthBarsPermanent = false;

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
