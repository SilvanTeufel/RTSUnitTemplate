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
#include "ControllerBase.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EGridShape : uint8
{
	Square,
	Staggered,
	VerticalLine
};


UCLASS()
class RTSUNITTEMPLATE_API AControllerBase : public APlayerController
{
	GENERATED_BODY()

public:
	AControllerBase();
	
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;


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
	// Function called by timer to display FPS
	void DisplayUnitCount();
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void ToggleUnitCountDisplay(bool bEnable);
	
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

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWaypoint* CreateAWaypoint(FVector NewWPLocation, ABuildingBase* BuildingBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool SetBuildingWaypoint(FVector NewWPLocation, AUnitBase* Unit, AWaypoint*& BuildingWaypoint, bool& PlayWaypointSound);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	float GridSpacing = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Settings")
	EGridShape GridFormationShape = EGridShape::Staggered;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 ComputeGridSize(int32 NumUnits) const;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector CalculateGridOffset(int32 Row, int32 Col) const;


	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DrawDebugCircleAtLocation(UWorld* World, const FVector& Location, FColor CircleColor);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector TraceRunLocation(FVector RunLocation, bool& HitNavModifier);
	
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
	void SpawnEffectArea(int TeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, AUnitBase* ActorToLockOn = nullptr);
	
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
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int SelectableTeamId = -1;

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
	void AddUnitToChase(AUnitBase* DetectingUnit, AActor* OtherActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveUnitToChase(AUnitBase* DetectingUnit, AActor* OtherActor);
};