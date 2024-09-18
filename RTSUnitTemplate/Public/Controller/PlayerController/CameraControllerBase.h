// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Controller/Input/EnhancedInputComponentBase.h"
#include "CameraControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACameraControllerBase : public AExtendedControllerBase
{
	GENERATED_BODY()
	
	ACameraControllerBase();

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void MoveCamToLocation(ACameraBase* Camera, const FVector& DestinationLocation);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool CheckSpeakingUnits();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetCameraState(TEnumAsByte<CameraData::CameraState> NewCameraState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GetViewPortScreenSizes(int x);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector GetCameraPanDirection();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector CalculateUnitsAverage(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GetAutoCamWaypoints();
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int UnitCountInRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int UnitCountToZoomOut = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int UnitZoomScaler = 10;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetCameraAveragePosition(ACameraBase* Camera, float DeltaTime);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float CalcControlTimer;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float OrbitAndMovePauseTime = 5.f;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float OrbitLocationControlTimer;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int OrbitRotatorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <FVector> OrbitPositions = { FVector(0.f), FVector(3000.f, 3000.f, 0.f) };
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <float> OrbitRadiuses = { 3000.f, 1000.f};
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <float> OrbitTimes = { 5.f, 5.f};

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = RTSUnitTemplate)
	float UnitCountOrbitTimeMultiplyer = 0.5f;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraBaseMachine(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RotateCam(float DeltaTime);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveCamToPosition(float DeltaSeconds, FVector Destination);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveCamToClick(float DeltaSeconds, FVector Destination);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveCam(float DeltaSeconds, FVector Destination);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OrbitAtLocation(FVector Destination, float OrbitSpeed);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ToggleLockCamToCharacter", Keywords = "TopDownRTSCamLib ToggleLockCamToCharacter"), Category = RTSUnitTemplate)
	void ToggleLockCamToCharacter();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnlockCamFromCharacter", Keywords = "TopDownRTSCamLib UnlockCamFromCharacter"), Category = RTSUnitTemplate)
	void UnlockCamFromCharacter();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LockCamToSpecificUnit", Keywords = "TopDownRTSCamLib LockCamToSpecificUnit"), Category = RTSUnitTemplate)
	void LockCamToSpecificUnit(AUnitBase* SUnit);

	ASpeakingUnit* SpeakingUnit;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LockCamToCharacter", Keywords = "TopDownRTSCamLib LockCamToCharacter"), Category = RTSUnitTemplate)
	void LockCamToCharacter(int Index);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LockCamToCharacter", Keywords = "TopDownRTSCamLib LockCamToCharacter"), Category = RTSUnitTemplate)
	void LockZDistanceToCharacter();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetCameraZDistance", Keywords = "TopDownRTSCamLib SetCameraZDistance"), Category = RTSUnitTemplate)
	void SetCameraZDistance(int Index);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "LockCameraToCharacter", Keywords = "TopDownRTSCamLib LockCameraToCharacter"), Category = RTSUnitTemplate)
	bool LockCameraToCharacter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RotateBehindCharacterIfLocked = true;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CamIsRotatingRight", Keywords = "TopDownRTSCamLib CamIsRotatingRight"), Category = RTSUnitTemplate)
	bool CamIsRotatingRight = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CamIsRotatingLeft", Keywords = "TopDownRTSCamLib CamIsRotatingLeft"), Category = RTSUnitTemplate)
	bool CamIsRotatingLeft = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CamIsZoomingInState", Keywords = "TopDownRTSCamLib CamIsZoomingInState"), Category = RTSUnitTemplate)
	int CamIsZoomingInState = 0;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "CamIsZoomingOutState", Keywords = "TopDownRTSCamLib CamIsZoomingOutState"), Category = RTSUnitTemplate)
	int CamIsZoomingOutState = 0;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "ZoomOutToPosition", Keywords = "TopDownRTSCamLib ZoomOutToPosition"), Category = RTSUnitTemplate)
	bool ZoomOutToPosition = false;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "ZoomInToPosition", Keywords = "TopDownRTSCamLib ZoomInToPosition"), Category = RTSUnitTemplate)
	bool ZoomInToPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AutoCamPlayerOnly = true;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "HoldZoomOnLockedCharacter", Keywords = "TopDownRTSCamLib HoldZoomOnLockedCharacter"), Category = RTSUnitTemplate)
	bool HoldZoomOnLockedCharacter = false;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "ScrollZoomCount", Keywords = "TopDownRTSCamLib ScrollZoomCount"), Category = RTSUnitTemplate)
	float ScrollZoomCount = 0.f;

};
