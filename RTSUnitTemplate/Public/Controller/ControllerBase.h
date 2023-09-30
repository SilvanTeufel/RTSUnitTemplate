// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hud/PathProviderHUD.h"
#include "Characters/CameraBase.h"
//#include "Core/DijkstraMatrix.h"
#include "GameFramework/PlayerController.h"
#include "Actors/EffectArea.h"
#include "ControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AControllerBase : public APlayerController
{
	GENERATED_BODY()

public:
	AControllerBase();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	APathProviderHUD* HUDBase;
	ACameraBase* CameraBase;
	
	void Tick(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShiftPressed", Keywords = "RTSUnitTemplate ShiftPressed"), Category = RTSUnitTemplate)
		void ShiftPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShiftReleased", Keywords = "RTSUnitTemplate ShiftReleased"), Category = RTSUnitTemplate)
		void ShiftReleased();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LeftClickPressed", Keywords = "RTSUnitTemplate LeftClickPressed"), Category = RTSUnitTemplate)
		void LeftClickPressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LeftClickReleased", Keywords = "RTSUnitTemplate LeftClickReleased"), Category = RTSUnitTemplate)
		void LeftClickReleased();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation);

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IsShiftPressed", Keywords = "RTSUnitTemplate IsShiftPressed"), Category = RTSUnitTemplate)
		bool UseUnrealEnginePathFinding = true;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RightClickPressed", Keywords = "RTSUnitTemplate RightClickPressed"), Category = RTSUnitTemplate)
		void RightClickPressed();

	UFUNCTION(meta = (DisplayName = "SetRunLocationUseDijkstra", Keywords = "RTSUnitTemplate SetRunLocationUseDijkstra"), Category = RTSUnitTemplate)
		void SetRunLocationUseDijkstra(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i);

	UFUNCTION(meta = (DisplayName = "SetRunLocationUseDijkstra", Keywords = "RTSUnitTemplate SetRunLocationUseDijkstra"), Category = RTSUnitTemplate)
		void SetRunLocationUseDijkstraForAI(FVector HitLocation, FVector UnitLocation, TArray <AUnitBase*> Units, TArray<FPathPoint>& PathPoints, int i);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpacePressed", Keywords = "RTSUnitTemplate SpacePressed"), Category = RTSUnitTemplate)
		void SpacePressed();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpaceReleased", Keywords = "RTSUnitTemplate SpaceReleased"), Category = RTSUnitTemplate)
		void SpaceReleased();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TSubclassOf<class AMissileRain> MissileRainClass;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SpawnMissileRain(int TeamId, FVector Location);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		TSubclassOf<class AEffectArea> EffectAreaClass;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SpawnEffectArea(int TeamId, FVector Location);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IsShiftPressed", Keywords = "RTSUnitTemplate IsShiftPressed"), Category = RTSUnitTemplate)
		bool IsShiftPressed = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "AttackToggled", Keywords = "RTSUnitTemplate AttackToggled"), Category = RTSUnitTemplate)
		bool AttackToggled = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IsStrgPressed", Keywords = "RTSUnitTemplate IsStrgPressed"), Category = RTSUnitTemplate)
		bool IsStrgPressed = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "IsSpacePressed", Keywords = "RTSUnitTemplate IsSpacePressed"), Category = RTSUnitTemplate)
		bool IsSpacePressed = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "AltIsPressed", Keywords = "RTSUnitTemplate AltIsPressed"), Category = RTSUnitTemplate)
		bool AltIsPressed = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "LeftClickisPressed", Keywords = "RTSUnitTemplate LeftClickisPressed"), Category = RTSUnitTemplate)
		bool LeftClickIsPressed = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "LockCameraToUnit", Keywords = "RTSUnitTemplate LockCameraToUnit"), Category = RTSUnitTemplate)
		bool LockCameraToUnit = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "AIsPressed", Keywords = "TopDownRTSCamLib AIsPressed"), Category = RTSUnitTemplate)
		int AIsPressedState = 0;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "DIsPressed", Keywords = "TopDownRTSCamLib DIsPressed"), Category = RTSUnitTemplate)
		int DIsPressedState = 0;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "WIsPressed", Keywords = "TopDownRTSCamLib WIsPressed"), Category = RTSUnitTemplate)
		int WIsPressedState = 0;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SIsPressed", Keywords = "TopDownRTSCamLib SIsPressed"), Category = RTSUnitTemplate)
		int SIsPressedState = 0;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
		bool MiddleMouseIsPressed = false;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
		TArray <AUnitBase*> SelectedUnits;

};
