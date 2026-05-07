// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "HUDBase.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	int32 Segments = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float SegmentSpace = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RadiusMultiplier = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor HealthColor = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor ShieldColor = FColor::Cyan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FColor BackgroundColor = FColor(0, 0, 0, 150);
};

UCLASS()
class RTSUNITTEMPLATE_API AHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	void AddClickIndicator(FVector Location, FColor Color, float LifeTime = -1.f, float Radius = -1.f);

private:
	struct FClickIndicator
	{
		FVector Location;
		FColor Color;
		float ExpiryTime;
		float Radius;
	};

	TArray<FClickIndicator> ClickIndicators;
	void DrawProjectedCircle(const FVector& Location, float Radius, FColor Color, float Thickness = -1.f, int32 InSegments = -1, bool bDisableSizeCulling = false);
	
	void DrawSelectionIndicator(class AUnitBase* Unit, const FVector& Location, float RadiusX, float RadiusY, const FRotator& Rotation, FLinearColor Color, float Thickness, bool bDisableOcclusion = false, int32 InSegments = -1);
	void DrawAllSelectedUnitsIndicators();
	void DrawAllHealthBars();
	void DrawStackedHealthBar(class AUnitBase* Unit, const FVector2D& ScreenPos, float WorldRadius, const FHealthBarSettings& Settings);
	void DrawSemiCircleHealthBar(class AUnitBase* Unit, const FVector& DrawLocation, float RadiusX, float RadiusY, bool bIsFlying, const FHealthBarSettings& Settings);
	void DrawSideBracketsHealthBar(class AUnitBase* Unit, const FVector2D& ScreenPos, float WorldRadius, float WorldWidthRadius, const FHealthBarSettings& Settings);

public:
	UFUNCTION(BlueprintCallable, Category = "RTS|HUD")
	void RegisterUnit(class AUnitBase* Unit);

	UFUNCTION(BlueprintCallable, Category = "RTS|HUD")
	void UnregisterUnit(class AUnitBase* Unit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	FColor SelectionColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	float SelectionThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	float SelectionSizeMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	TEnumAsByte<ESelectionIndicatorStyle> SelectionStyle = Circle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	bool bEnableOcclusion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	bool bEnableStandardSelection = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	bool bEnableHealthBars = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	bool bShowAllHealthBarsPermanent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	bool bShowHealthOnSelected = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings BuildingHealthBarSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings FlyingHealthBarSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|HUD|Health")
	FHealthBarSettings GroundHealthBarSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Selection")
	float RotatingCircleSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ClickIndicatorRadius = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ClickIndicatorThickness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ClickIndicatorLifeTime = 1.5f;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	virtual void DrawHUD();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SelectISMUnitsInRectangle(const FVector2D& RectMin, const FVector2D& RectMax);
	
	void Tick(float DeltaSeconds);
	void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bSelectFullSquad = false;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SelectUnitsFromSameSquad(AUnitBase* SelectedUnit);

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AUnitBase*> FriendlyUnits;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AUnitBase*> EnemyUnitBases;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddUnitsToArray();
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "bSelectFriendly", Keywords = "TopDownRTSTemplate bSelectFriendly"), Category = TopDownRTSTemplate)
	bool bSelectFriendly = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "InitialPoint", Keywords = "RTSUnitTemplate InitialPoint"), Category = RTSUnitTemplate)
	FVector2D InitialPoint;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CurrentPoint", Keywords = "RTSUnitTemplate CurrentPoint"), Category = RTSUnitTemplate)
	FVector2D CurrentPoint;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IPoint", Keywords = "RTSUnitTemplate IPoint"), Category = RTSUnitTemplate)
	FVector IPoint = FVector(0.f,0.f, 0.f);

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CPoint", Keywords = "RTSUnitTemplate CPoint"), Category = RTSUnitTemplate)
	FVector CPoint = FVector(0.f,0.f, 0.f);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "RectangleScaleSelectionFactor", Keywords = "RTSUnitTemplate RectangleScaleSelectionFactor"), Category = RTSUnitTemplate)
	float RectangleScaleSelectionFactor = 0.9;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMousePos2D", Keywords = "RTSUnitTemplate GetMousePos2D"), Category = RTSUnitTemplate)
	FVector2D GetMousePos2D();

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SelectedEnemyUnits", Keywords = "RTSUnitTemplate SelectedEnemyUnits"), Category = RTSUnitTemplate)
	TArray <AUnitBase*> SelectedUnits;

	UPROPERTY()
	TSet <AUnitBase*> SelectedUnitsSet;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsSpeakingUnitClose", Keywords = "RTSUnitTemplate IsSpeakingUnitClose"), Category = TopDownRTSTemplate)
	void IsSpeakingUnitClose(TArray <AUnitBase*> Units, TArray <ASpeakingUnit*> SpeakUnits);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CharacterIsUnSelectable", Keywords = "RTSUnitTemplate CharacterIsUnSelectable"), Category = RTSUnitTemplate)
	bool CharacterIsUnSelectable = true;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveUnitsThroughWayPoints", Keywords = "RTSUnitTemplate MoveUnitsThroughWayPoints"), Category = TopDownRTSTemplate)
	void MoveUnitsThroughWayPoints(TArray <AUnitBase*> Units);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "PatrolUnitsThroughWayPoints", Keywords = "RTSUnitTemplate PatrolUnitsThroughWayPoints"), Category = TopDownRTSTemplate)
	void PatrolUnitsThroughWayPoints(TArray <AUnitBase*> Units);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetUnitSelected", Keywords = "RTSUnitTemplate SetUnitSelected"), Category = TopDownRTSTemplate)
	void SetUnitSelected(AUnitBase* Unit, bool bIsAi = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DeselectAllUnits", Keywords = "RTSUnitTemplate DeselectAllUnits"), Category = TopDownRTSTemplate)
	void DeselectAllUnits();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DetectUnit", Keywords = "RTSUnitTemplate DetectUnit"), Category = TopDownRTSTemplate)
	void DetectUnit(AUnitBase* DetectingUnit, TArray <AActor*> &DetectedUnits, float Sight, float LoseSight, bool DetectFriendlyUnits, int PlayerTeamId);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ControllDirectionToMouse", Keywords = "RTSUnitTemplate ControllDirectionToMouse"), Category = TopDownRTSTemplate)
	void ControllDirectionToMouse(AActor* Units, FHitResult Hit);

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

	void DrawDashedLine3D(const FVector& Start, const FVector& End, float DashLen, float GapLen, FColor Color, float Thickness, float ZOffset);

private:
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
};
