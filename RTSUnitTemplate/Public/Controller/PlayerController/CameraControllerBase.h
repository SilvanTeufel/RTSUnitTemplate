// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Controller/Input/EnhancedInputComponentBase.h"
#include "CameraControllerBase.generated.h"

class ULoadingWidget;

USTRUCT(BlueprintType)
struct FLoadingWidgetConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<class ULoadingWidget> WidgetClass = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	float Duration = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	int32 TriggerId = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	float ServerWorldTimeStart = -1.f;
};

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACameraControllerBase : public ACustomControllerBase
{
	GENERATED_BODY()
	
	public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_TravelToMap(const FString& MapName, FName TagToEnable = NAME_None);


	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpdateCameraUnitMovement(const FVector& TargetLocation);

	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	FVector LastCameraUnitMovementLocation = FVector::ZeroVector;
	
	UPROPERTY()
	class ULoadingWidget* ActiveLoadingWidget = nullptr;

	int32 LastProcessedLoadingTriggerId = -1;

	UFUNCTION(Client, Reliable)
	void Client_TriggerWinLoseUI(bool bWon, TSubclassOf<class UWinLoseWidget> InWidgetClass, const FString& InMapName, FName DestinationSwitchTagToEnable);

	UFUNCTION(Client, Reliable)
	void Client_InitializeWinLoseSystem();

	FTimerHandle WinLoseTimerHandle;

	UFUNCTION(Client, Reliable)
	void Client_ShowLoadingWidget(TSubclassOf<class ULoadingWidget> InClass, float InTotalDuration, float InServerWorldTimeStart, int32 InTriggerId);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void CheckForLoadingWidget();

	void Retry_ShowLoadingWidget(TSubclassOf<class ULoadingWidget> InClass, float InTotalDuration, float InServerWorldTimeStart, int32 InTriggerId, int32 RetryCount);

	ACameraControllerBase();

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SetCameraUnitWithTag(FGameplayTag Tag, int TeamId);

	UFUNCTION(Client, Reliable)
	void ClientSetCameraUnit(AUnitBase* CameraUnit, int TeamId);

	UFUNCTION(Server, Reliable)
	void ServerSetCameraUnit(AUnitBase* CameraUnit, int TeamId);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetCameraOnly();

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
	
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate")
	int UnitCountInRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	int UnitCountToZoomOut = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
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
	void StopAllCameraMovement();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_UseScreenEdges();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_MoveWASD(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ZoomIn();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ZoomOut();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ScrollZoomIn();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ScrollZoomOut();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ZoomOutPosition();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ZoomInPosition();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_HoldRotateLeft();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_HoldRotateRight();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_RotateLeft();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_RotateRight();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_LockOnCharacter();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_LockOnCharacterWithTag(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_LockOnSpeaking();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ZoomToNormalPosition();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ZoomToThirdPerson();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_ThirdPerson();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_RotateToStart();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_MoveToPosition(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_OrbitAtPosition();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_MoveToClick(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_LockOnActor();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CameraState_OrbitAndMove(float DeltaTime);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ASpeakingUnit* SpeakingUnit;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LockCamToCharacter(int Index);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void MoveAndRotateUnit(AUnitBase* Unit, const FVector& Direction, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LocalMoveAndRotateUnit(AUnitBase* Unit, const FVector& Direction, float DeltaTime);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LockCamToCharacterWithTag(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveCameraUnit();


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetCameraUnitTransform(FVector TargetLocation, FRotator TargetRotation);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector CameraUnitMovementLocation = FVector::ZeroVector;
	
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_MoveInDirection(FVector Direction, float DeltaTime);

	UFUNCTION(Server, Unreliable)
	void Server_SyncCameraPosition(FVector NewPosition);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_RotateCamera(float Direction, float Add, bool stopCam);

	UFUNCTION(Server, Reliable)
	void Server_MoveCamToPosition(float DeltaSeconds, FVector Destination);

	UFUNCTION(Server, Reliable)
	void Server_SetCameraLocation(FVector NewLocation);

	UFUNCTION(Server, Reliable)
	void Server_MoveCamToClick(float DeltaSeconds, FVector Destination);

	UFUNCTION(Server, Reliable)
	void Server_MoveCam(float DeltaSeconds, FVector Destination);

	UFUNCTION(Server, Reliable)
	void Server_RotateSpringArm(bool Invert);

	UFUNCTION(Server, Reliable)
	void Server_ZoomIn(float Value, bool Stop);

	UFUNCTION(Server, Reliable)
	void Server_ZoomOut(float Value, bool Stop);

	UFUNCTION(Server, Reliable)
	void Server_ZoomInToPosition(float Distance, FVector OptionalLocation);

	UFUNCTION(Server, Reliable)
	void Server_ZoomOutToPosition(float Distance, FVector OptionalLocation);

	UFUNCTION(Server, Reliable)
	void Server_ZoomInToThirdPerson(FVector SelectedActorLocation);

	UFUNCTION(Server, Reliable)
	void Server_ZoomOutAutoCam(float Position);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LockZDistanceToCharacter();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetCameraZDistance(int Index);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	bool LockCameraToCharacter = false;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bIsCameraMovementHaltedByUI = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RotateBehindCharacterIfLocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CameraUnitMouseFollow = true;
	
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
	
private:
	static bool bServerTravelInProgress;

	// Helper functions for scroll zoom logic
	void HandleScrollZoomIn();
	void HandleScrollZoomOut();

};
