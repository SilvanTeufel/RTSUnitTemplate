// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Controller/Input/InputConfig.h"
#include "InputActionValue.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "InputMappingContext.h"
#include "CameraBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API ACameraBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	//ACameraBase(const FObjectInitializer& ObjectInitializer);

	UCapsuleComponent* GetCameraBaseCapsule() const {
		return GetCapsuleComponent();
	}

	bool bLockCameraZRotation = true;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputConfig* InputConfig;

	bool BlockControls = true;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetActorBasicLocation();
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
		void CreateCameraComp();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "RootScene", Keywords = "RTSUnitTemplate RootScene"), Category = RTSUnitTemplate)
		USceneComponent* RootScene;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SpringArm", Keywords = "RTSUnitTemplate SpringArm"), Category = RTSUnitTemplate)
		USpringArmComponent* SpringArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SpringArmRotator", Keywords = "RTSUnitTemplate SpringArmRotator"), Category = RTSUnitTemplate)
		FRotator SpringArmRotator = FRotator(-50, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "CameraComp", Keywords = "RTSUnitTemplate CameraComp"), Category = RTSUnitTemplate)
		UCameraComponent* CameraComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "MappingContext", Keywords = "TopDownRTSCamLib MappingContext"), Category = TopDownRTSCamLib)
		UInputMappingContext* MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "MappingPriority", Keywords = "TopDownRTSCamLib MappingPriority"), Category = TopDownRTSCamLib)
		int32 MappingPriority;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "CameraDistanceToCharacter", Keywords = "TopDownRTSCamLib CameraDistanceToCharacter"), Category = TopDownRTSCamLib)
		float CameraDistanceToCharacter = 1500.f;

	UFUNCTION( BlueprintCallable, meta = (DisplayName = "PanMoveCamera", Keywords = "RTSUnitTemplate PanMoveCamera"), Category = RTSUnitTemplate)
		void PanMoveCamera(const FVector& NewPanDirection);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Margin", Keywords = "RTSUnitTemplate Margin"), Category = RTSUnitTemplate)
		float Margin = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ScreenSizeX", Keywords = "RTSUnitTemplate ScreenSizeX"), Category = RTSUnitTemplate)
		int32 ScreenSizeX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ScreenSizeY", Keywords = "RTSUnitTemplate ScreenSizeY"), Category = RTSUnitTemplate)
		int32 ScreenSizeY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "GetViewPortScreenSizesState", Keywords = "RTSUnitTemplate GetViewPortScreenSizesState"), Category = RTSUnitTemplate)
		int GetViewPortScreenSizesState = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		FVector CurrentCamSpeed = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float CamActorRespawnZLocation = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ForceRespawnZLocation = -100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float AccelerationRate = 7500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float DecelerationRate = 15000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		float CamSpeed = 7000.f; // 120

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		float ZoomSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		float FastZoomSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		float AutoZoomSpeed = 25.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ZoomAccelerationRate = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ZoomDecelerationRate = 15.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "EdgeScrollCamSpeed", Keywords = "RTSUnitTemplate EdgeScrollCamSpeed"), Category = RTSUnitTemplate)
		float EdgeScrollCamSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpringArmMinRotator = -10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpringArmMaxRotator = -50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpringArmStartRotator = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpringArmRotatorSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpringArmRotatorMaxSpeed = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpringArmRotatorAcceleration = 0.05f;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void RotateSpringArm(bool Invert = false);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomIn", Keywords = "RTSUnitTemplate ZoomIn"), Category = RTSUnitTemplate)
		void ZoomIn(float ZoomMultiplier, bool Decelerate = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomOut", Keywords = "RTSUnitTemplate ZoomOut"), Category = RTSUnitTemplate)
		void ZoomOut(float ZoomMultiplier, bool Decelerate = false);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void AdjustSpringArmRotation(float Difference, float& OutRotationValue);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool RotateFree(FVector MouseLocation);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotateCamLeft", Keywords = "RTSUnitTemplate RotateCamLeft"), Category = TopDownRTSCamLib)
		bool RotateCamLeft(float Add, bool stopCam = false); // CamRotationOffset
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotateCamRight", Keywords = "RTSUnitTemplate RotateCamRight"), Category = TopDownRTSCamLib)
		bool RotateCamRight(float Add, bool stopCam = false); // CamRotationOffset
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotateCamLeft", Keywords = "RTSUnitTemplate RotateCamLeft"), Category = TopDownRTSCamLib)
		bool RotateCamLeftTo(float Position, float Add);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotateCamRight", Keywords = "RTSUnitTemplate RotateCamRight"), Category = TopDownRTSCamLib)
		bool RotateCamRightTo(float Position, float Add);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "JumpCamera", Keywords = "RTSUnitTemplate JumpCamera"), Category = RTSUnitTemplate)
		void JumpCamera(FHitResult Hit);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool ZoomOutAutoCam(float Distance, const FVector SelectedActorPosition = FVector(0.f,0.f,0.f));

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomOutToPosition", Keywords = "RTSUnitTemplate ZoomOutToPosition"), Category = RTSUnitTemplate)
		bool ZoomOutToPosition(float Distance, const FVector SelectedActorPosition = FVector(0.f,0.f,0.f));
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomInToPosition", Keywords = "RTSUnitTemplate ZoomInToPosition"), Category = RTSUnitTemplate)
		bool ZoomInToPosition(float Distance, const FVector SelectedActorPosition = FVector(0.f,0.f,0.f));

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LockOnUnit", Keywords = "RTSUnitTemplate LockOnUnit"), Category = RTSUnitTemplate)
		void LockOnUnit(AUnitBase* Unit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ZoomOutPosition", Keywords = "RTSUnitTemplate ZoomOutPosition"), Category = RTSUnitTemplate)
		float ZoomOutPosition = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ZoomPosition", Keywords = "RTSUnitTemplate ZoomPosition"), Category = RTSUnitTemplate)
		float ZoomPosition = 75.f; // 1500.f

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "PitchValue", Keywords = "RTSUnitTemplate PitchValue"), Category = RTSUnitTemplate)
		float PitchValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "YawValue", Keywords = "RTSUnitTemplate YawValue"), Category = RTSUnitTemplate)
		float YawValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float CurrentRotationValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float RotationIncreaser = 0.01f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float RollValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool AutoLockOnSelect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool DisableEdgeScrolling = false;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomThirdPersonPosition", Keywords = "TopDownRTSCamLib ZoomThirdPersonPosition"), Category = RTSUnitTemplate)
		bool IsCharacterDistanceTooLow(float Distance, const FVector SelectedActorPosition = FVector(0.f,0.f,0.f));

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsCharacterDistanceTooHigh", Keywords = "TopDownRTSCamLib IsCharacterDistanceTooHigh"), Category = RTSUnitTemplate)
		bool IsCharacterDistanceTooHigh(float Distance, const FVector SelectedActorPosition);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ZoomThirdPersonPosition", Keywords = "TopDownRTSCamLib ZoomThirdPersonPosition"), Category = RTSUnitTemplate)
		bool ZoomInToThirdPerson(const FVector SelectedActorPosition = FVector(0.f,0.f,0.f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ZoomThirdPersonPosition", Keywords = "TopDownRTSCamLib ZoomThirdPersonPosition"), Category = RTSUnitTemplate)
		float ZoomThirdPersonPosition = 600.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "CamRotationOffset", Keywords = "TopDownRTSCamLib CamRotationOffset"), Category = RTSUnitTemplate)
		float CamRotationOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "AddCamRotation", Keywords = "TopDownRTSCamLib AddCamRotation"), Category = RTSUnitTemplate)
		float AddCamRotation = 0.75f;
	
	UPROPERTY(EditAnywhere, meta = (DisplayName = "CameraAngles", Keywords = "TopDownRTSCamLib CameraAngles"), Category = RTSUnitTemplate)
		float CameraAngles[4] = { 0.f, 90.f, 180.f, 270.f };

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "RotationDegreeStep", Keywords = "TopDownRTSCamLib RotationDegreeStep"), Category = RTSUnitTemplate)
		float RotationDegreeStep = 90.f;
	
	bool IsCameraInAngle()
	{
		if(SpringArmRotator.Yaw == 360.f) SpringArmRotator.Yaw = 0.f;
		bool IsInAngle = false;
		for( int i = 0; i < 4 ; i++)
		{
			if(SpringArmRotator.Yaw == CameraAngles[i]) IsInAngle = true;
		}
		return IsInAngle;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveCamToForward", Keywords = "TopDownRTSCamLib MoveCamToForward"), Category = RTSUnitTemplate)
		void MoveCamToForward(float DeltaTime, bool Decelerate = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveCamToBackward", Keywords = "TopDownRTSCamLib MoveCamToBackward"), Category = RTSUnitTemplate)
		void MoveCamToBackward(float DeltaTime, bool Decelerate = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveCamToLeft", Keywords = "TopDownRTSCamLib MoveCamToLeft"), Category = RTSUnitTemplate)
		void MoveCamToLeft(float DeltaTime, bool Decelerate = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "MoveCamToRight", Keywords = "TopDownRTSCamLib MoveCamToRight"), Category = RTSUnitTemplate)
		void MoveCamToRight(float DeltaTime, bool Decelerate = false);
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "StartTime", Keywords = "TopDownRTSCamLib StartTime"), Category = RTSUnitTemplate)
		float StartTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "CameraState", Keywords = "TopDownRTSTemplate CameraState"), Category = RTSUnitTemplate)
		TEnumAsByte<CameraData::CameraState> CameraState = CameraData::UseScreenEdges;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetCameraState", Keywords = "TopDownRTSCamLib SetCameraState"), Category = RTSUnitTemplate)
		void SetCameraState(TEnumAsByte<CameraData::CameraState> NewCameraState);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetCameraState", Keywords = "TopDownRTSCamLib GetCameraState"), Category = RTSUnitTemplate)
		TEnumAsByte<CameraData::CameraState> GetCameraState();



	// Orbit
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OrbitCamLeft", Keywords = "RTSUnitTemplate OrbitCamLeft"), Category = TopDownRTSCamLib)
	bool OrbitCamLeft(float Add);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector OrbitLocation = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float OrbitRotationValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float OrbitIncreaser = 0.0001f; //0.0001f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float OrbitSpeed = 0.033f; //0.0010f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MovePositionCamSpeed = 1.0f;
	
	// Control Widget

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (DisplayName = "ControlWidgetComp", Keywords = "RTSUnitTemplate ControlWidgetComp"), Category = RTSUnitTemplate)
		class UWidgetComponent* ControlWidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ControlWidgetRotator", Keywords = "RTSUnitTemplate ControlWidgetRotator"), Category = RTSUnitTemplate)
		FRotator ControlWidgetRotation = FRotator(50, 180, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ControlWidgetLocation", Keywords = "TopDownRTSTemplate ControlWidgetLocation"), Category = TopDownRTSTemplate)
		FVector2D ControlWidgetLocation = FVector2D(0.5f, 0.5f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ControlWidgetHideLocation", Keywords = "RTSUnitTemplate ControlWidgetHideLocation"), Category = RTSUnitTemplate)
		FVector ControlWidgetHideLocation = FVector(400.f, -2500.0f, -250.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
		bool ShowControlWidgetAtStart = true;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "HideControlWidget", Keywords = "RTSUnitTemplate HideControlWidget"), Category = RTSUnitTemplate)
		void HideControlWidget();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShowControlWidget", Keywords = "RTSUnitTemplate ShowControlWidget"), Category = RTSUnitTemplate)
		void ShowControlWidget();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetControlWidgetLocation();
	
	/////// Loading Widget ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (DisplayName = "LoadingWidgetComp", Keywords = "RTSUnitTemplate LoadingWidgetComp"), Category = RTSUnitTemplate)
	class UWidgetComponent* LoadingWidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "LoadingWidgetRotation", Keywords = "RTSUnitTemplate LoadingWidgetRotation"), Category = RTSUnitTemplate)
	FRotator LoadingWidgetRotation = FRotator(50.f, 180, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "LoadingWidgetLocation", Keywords = "RTSUnitTemplate LoadingWidgetLocation"), Category = RTSUnitTemplate)
	FVector LoadingWidgetLocation = FVector(150.f, 000.0f, -150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "LoadingWidgetHideLocation", Keywords = "RTSUnitTemplate LoadingWidgetHideLocation"), Category = RTSUnitTemplate)
	FVector LoadingWidgetHideLocation = FVector(300.f, -2200.0f, -250.0f);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "HideLoadingWidget", Keywords = "RTSUnitTemplate HideLoadingWidget"), Category = RTSUnitTemplate)
		void DeSpawnLoadingWidget();
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpawnLoadingWidget", Keywords = "RTSUnitTemplate SpawnLoadingWidget"), Category = RTSUnitTemplate)
		void SpawnLoadingWidget();
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


};
