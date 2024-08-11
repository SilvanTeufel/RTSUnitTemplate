// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Camera/CameraBase.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Controller/Input/EnhancedInputComponentBase.h"
#include "Controller/Input/GameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UnrealEngine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetMathLibrary.h"


// Called when the game starts or when spawned
void ACameraBase::BeginPlay()
{
	Super::BeginPlay();

	
	if(ControlWidgetComp)
		ControlWidgetComp->SetVisibility(false);
		
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		
		// Get the Enhanced Input Local Player Subsystem from the Local Player related to our Player Controller.
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// PawnClientRestart can run more than once in an Actor's lifetime, so start by clearing out any leftover mappings.
			Subsystem->ClearAllMappings();

			// Add each mapping context, along with their priority values. Higher values outprioritize lower values.
			Subsystem->AddMappingContext(MappingContext, MappingPriority);
		}
		
	}
	
}

void ACameraBase::SetActorBasicLocation()
{
	FVector ActorLocation = GetActorLocation();

	if(ActorLocation.Z < ForceRespawnZLocation)
	SetActorLocation(FVector(ActorLocation.X, ActorLocation.Y, CamActorRespawnZLocation));
}
// Called every frame
void ACameraBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SetActorBasicLocation();

	if(ShowControlWidgetAtStart) ShowControlWidget();
}

void ACameraBase::CreateCameraComp()
{
	RootScene = RootComponent;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCameraBaseCapsule()); // RootScene // GetCapsuleComponent()
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->SetRelativeRotation(SpringArmRotator);

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArm);
}


void ACameraBase::PanMoveCamera(const FVector& NewPanDirection) {
	if (NewPanDirection != FVector::ZeroVector) {
		AddActorWorldOffset(NewPanDirection * GetActorLocation().Z * 0.001);
	}
}

void ACameraBase::RotateSpringArm(bool Invert)
{
	if(!Invert && SpringArmRotator.Pitch <= SpringArmMinRotator && SpringArm->TargetArmLength < SpringArmStartRotator)
	{
		if(SpringArmRotatorSpeed <= 0.f)
			SpringArmRotatorSpeed = 0.f;	
		
		if(SpringArmRotatorSpeed < SpringArmRotatorMaxSpeed)
		SpringArmRotatorSpeed += SpringArmRotatorAcceleration;
		
		SpringArmRotator.Pitch += SpringArmRotatorSpeed;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}else if(Invert && SpringArmRotator.Pitch >= SpringArmMaxRotator)
	{
		if(SpringArmRotatorSpeed >= 0.f)
			SpringArmRotatorSpeed = 0.f;	
			
		if(SpringArmRotatorSpeed > -SpringArmRotatorMaxSpeed)
			SpringArmRotatorSpeed -= SpringArmRotatorAcceleration;
		
		SpringArmRotator.Pitch += SpringArmRotatorSpeed;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}else
	{
		SpringArmRotatorSpeed = 0.f;
	}
	
}
 
void ACameraBase::ZoomOut(float ZoomMultiplier, bool Decelerate) {

	if(!Decelerate && CurrentCamSpeed.Z >= ZoomSpeed*(-1))
		CurrentCamSpeed.Z -= 0.1f*ZoomAccelerationRate;
	else if(Decelerate && CurrentCamSpeed.Z < 0.f)
		CurrentCamSpeed.Z += 0.1f*ZoomDecelerationRate;
	else if(Decelerate)
		CurrentCamSpeed.Z = 0.f;
	
	float zoomAmount = 0.3f * (-1) * CurrentCamSpeed.Z * ZoomMultiplier;
	
	if(SpringArm)
		SpringArm->TargetArmLength += zoomAmount;

	//SetControlWidgetLocation();
}

void ACameraBase::ZoomIn(float ZoomMultiplier, bool Decelerate) {

	if(!Decelerate && CurrentCamSpeed.Z <= ZoomSpeed)
		CurrentCamSpeed.Z += 0.1f*ZoomAccelerationRate;
	else if(Decelerate && CurrentCamSpeed.Z > 0.f)
		CurrentCamSpeed.Z -= 0.1f*ZoomDecelerationRate;
	else if(Decelerate)
		CurrentCamSpeed.Z = 0.f;


	float zoomAmount = 0.3f * (-1) * CurrentCamSpeed.Z * ZoomMultiplier;

	
	
	if(SpringArm && SpringArm->TargetArmLength > 100.f)
		SpringArm->TargetArmLength += zoomAmount;

	//SetControlWidgetLocation();
}


void ACameraBase::AdjustSpringArmRotation(float Difference, float& OutRotationValue)
{
	if (Difference > 0) // wenn Difference positiv ist
	{
			OutRotationValue += RotationIncreaser*100;
	}
	else 
	{
			OutRotationValue -= RotationIncreaser*100;
	}
}


void ACameraBase::RotateSpringArmPitchFree(bool Invert)
{
	if(!Invert && SpringArmRotator.Pitch <= SpringArmMinRotator && SpringArm->TargetArmLength < SpringArmStartRotator)
	{
		SpringArmRotator.Pitch += 1.5f;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}else if(Invert && SpringArmRotator.Pitch >= SpringArmMaxRotator)
	{
		SpringArmRotator.Pitch -= 1.5f;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}else
	{
		SpringArmRotatorSpeed = 0.f;
	}
	
}

void ACameraBase::RotateSpringArmYawFree(bool Invert)
{
	if(!Invert)
	{
		SpringArmRotator.Yaw += 1.5f;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}
	else if(Invert)
	{
		SpringArmRotator.Yaw -= 1.5f;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}
	else
	{
		SpringArmRotatorSpeed = 0.f;
	}
}

bool ACameraBase::RotateFree(FVector MouseLocation)
{
	// Assume PreviousMouseLocation is a member variable that tracks the last mouse position
	FVector Delta = MouseLocation - PreviousMouseLocation;
	FVector Direction = Delta.GetSafeNormal();
	// Determine rotation direction based on mouse movement
	// Here, I assume horizontal rotation. You might need to adjust for vertical rotation
	//RotateSpringArm(false);
	float MindDelta = 100.f;
	
	if (Delta.Y > MindDelta) {
		// Mouse moved right
		RotateSpringArmPitchFree(false); // Rotate in one direction
	} else if (Delta.Y < -MindDelta) {
		// Mouse moved left
		RotateSpringArmPitchFree(true); // Rotate in the opposite direction
	}

	if (Delta.X > MindDelta) {
		// Mouse moved right
		RotateSpringArmYawFree(true); // Rotate in one direction
	} else if (Delta.X < -MindDelta) {
		// Mouse moved left
		RotateSpringArmYawFree(false); // Rotate in the opposite direction
	}
	
	// Update the previous mouse position for the next call
	PreviousMouseLocation += Direction;
	// You might want to return true if rotation happened, or false otherwise
	return Delta.X != 0;
}



bool ACameraBase::RotateCamLeft(float Add, bool stopCam) // CamRotationOffset
{

	if(stopCam && CurrentRotationValue > 0.f)
	{
		if(FMath::IsNearlyEqual(CurrentRotationValue, 0.f, RotationIncreaser*3))
			CurrentRotationValue = 0.0000f;
		else
			CurrentRotationValue -= RotationIncreaser*3;
		
	}else if(CurrentRotationValue < Add)
		CurrentRotationValue += RotationIncreaser;
	
	SpringArmRotator.Yaw += CurrentRotationValue;

	if(CurrentRotationValue >= 1.0f)
		SpringArmRotator.Yaw = floor(SpringArmRotator.Yaw+0.5);

	SpringArm->SetRelativeRotation(SpringArmRotator);

	//SetControlWidgetLocation();
	
	if (SpringArmRotator.Yaw >= 360) SpringArmRotator.Yaw = 0.f;
	
	if (FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[0], RotationIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[3], RotationIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[2], RotationIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[1], RotationIncreaser))
	{
		return true;
	}
	
	return false;
}

bool ACameraBase::RotateCamRight(float Add, bool stopCam) // CamRotationOffset
{
	if(stopCam && CurrentRotationValue > 0.f)
	{
		if(FMath::IsNearlyEqual(CurrentRotationValue, 0.f, RotationIncreaser*3))
			CurrentRotationValue = 0.0000f;
		else
			CurrentRotationValue -= RotationIncreaser*3;
		
	}else if(CurrentRotationValue < Add)
		CurrentRotationValue += RotationIncreaser;
	
	SpringArmRotator.Yaw -= CurrentRotationValue;

	if(CurrentRotationValue >= 1.0f)
		SpringArmRotator.Yaw = floor(SpringArmRotator.Yaw+0.5);

	SpringArm->SetRelativeRotation(SpringArmRotator);

	//SetControlWidgetLocation();
	
	if (SpringArmRotator.Yaw <= -1) SpringArmRotator.Yaw = 359.f;

	if (FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[0], RotationIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[3], RotationIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[2], RotationIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[1], RotationIncreaser))
	{
		return true;
	}

	
	return false;
}

bool ACameraBase::OrbitCamLeft(float Add)
{


	if(OrbitRotationValue < Add)
		OrbitRotationValue += OrbitIncreaser;
	
	SpringArmRotator.Yaw += OrbitRotationValue;


	
	if (SpringArmRotator.Yaw >= 360) SpringArmRotator.Yaw = 0.f;

	SpringArm->SetRelativeRotation(SpringArmRotator);
	
	if (FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[0], OrbitIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[3], OrbitIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[2], OrbitIncreaser) ||
	FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[1], OrbitIncreaser))
	{
		return true;
	}
		
	
	return false;
}

bool ACameraBase::RotateCamLeftTo(float Position, float Add)
{
	if (abs(SpringArmRotator.Yaw - Position) <= 1.f) return true;
	
	SpringArmRotator.Yaw += Add;
	if (SpringArmRotator.Yaw == 360) SpringArmRotator.Yaw = 0.f;
	if (SpringArmRotator.Yaw > CameraAngles[3]+RotationDegreeStep) SpringArmRotator.Yaw -= CameraAngles[3]+RotationDegreeStep;

	SpringArm->SetRelativeRotation(SpringArmRotator);
	
	return false;
}

bool ACameraBase::RotateCamRightTo(float Position, float Add)
{
	
	if (abs(SpringArmRotator.Yaw - Position) <= 1.f) return true;
	
	SpringArmRotator.Yaw -= Add;
	if (SpringArmRotator.Yaw == -1) SpringArmRotator.Yaw = 359.f;
	if (SpringArmRotator.Yaw < CameraAngles[0]) SpringArmRotator.Yaw += CameraAngles[3]+RotationDegreeStep;

	SpringArm->SetRelativeRotation(SpringArmRotator);
	
	return false;
}

bool ACameraBase::ZoomOutAutoCam(float Distance, const FVector SelectedActorPosition)
{


	if (SpringArm && SpringArm->TargetArmLength - SelectedActorPosition.Z < Distance)
	{
		SpringArm->TargetArmLength += AutoZoomSpeed/10;
		
		return false;
	}

	return true;
}

bool ACameraBase::ZoomOutToPosition(float Distance, const FVector SelectedActorPosition)
{


	if (SpringArm && SpringArm->TargetArmLength - SelectedActorPosition.Z < Distance)
	{
			SpringArm->TargetArmLength += FastZoomSpeed;
		
		return false;
	}

	return true;
}

bool ACameraBase::ZoomInToPosition(float Distance, const FVector SelectedActorPosition)
{

	if (SpringArm && SpringArm->TargetArmLength > 100.f && SpringArm->TargetArmLength - SelectedActorPosition.Z > Distance)
	{

		SpringArm->TargetArmLength -= FastZoomSpeed;
		
		return false;
	}
	return true;
}

void ACameraBase::LockOnUnit(AUnitBase* Unit)
{
	if (Unit && Unit->GetUnitState() != UnitData::Dead) {
		FVector ActorLocation = Unit->GetActorLocation();

		float ZLocation = GetActorLocation().Z;

		if(abs(ZLocation-ActorLocation.Z) >= 100.f) ZLocation = ActorLocation.Z;
		
		SetActorLocation(FVector(ActorLocation.X, ActorLocation.Y, ZLocation));
	}else
	{
		SetCameraState(CameraData::UseScreenEdges);
	}
}

void ACameraBase::LockOnActor(AActor* Actor)
{
	if (Actor) {
		FVector ActorLocation = Actor->GetActorLocation();

		float ZLocation = GetActorLocation().Z;

		//if(abs(ZLocation-ActorLocation.Z) >= 100.f) ZLocation = ActorLocation.Z;
		
		SetActorLocation(FVector(ActorLocation.X, ActorLocation.Y, GetActorLocation().Z));
	}else
	{
		SetCameraState(CameraData::UseScreenEdges);
	}
}

bool ACameraBase::IsCharacterDistanceTooLow(float Distance, const FVector SelectedActorPosition)
{
	if (!SpringArm) return false;

	float currentDistance = SpringArm->TargetArmLength + GetActorLocation().Z;
	if (currentDistance - SelectedActorPosition.Z < Distance)
	{
		return true;
	}
	return false;
}

bool ACameraBase::IsCharacterDistanceTooHigh(float Distance, const FVector SelectedActorPosition)
{
	if (!SpringArm) return false;

	float currentDistance = SpringArm->TargetArmLength + GetActorLocation().Z;
	if (currentDistance - SelectedActorPosition.Z > Distance)
	{
		return true;
	}
	return false;
}

bool ACameraBase::ZoomInToThirdPerson(const FVector SelectedActorPosition)
{

	if (SpringArm && SpringArm->TargetArmLength > 100.f && SpringArm->TargetArmLength - SelectedActorPosition.Z > ZoomThirdPersonPosition) {

		SpringArm->TargetArmLength -= FastZoomSpeed;
		
		return false;
	}
	return true;
}

void ACameraBase::HideControlWidget()
{
	if (ControlWidgetComp)
		ControlWidgetComp->SetVisibility(false);

	ShowControlWidgetAtStart = false;
}

void ACameraBase::ShowControlWidget()
{
	if (ControlWidgetComp)
	{
		ControlWidgetComp->SetVisibility(true);
		//SetControlWidgetLocation();
	}
}
/*
void ACameraBase::SetControlWidgetLocation()
{
	if (!ControlWidgetComp || !GetWorld()) return;

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController) return;
	
	FVector2d ScreenPosition = ControlWidgetLocation;
	
	FIntPoint ViewportSize;
	PlayerController->GetViewportSize(ViewportSize.X, ViewportSize.Y);

	// Ensure ScreenPosition is in the range [0, 1] representing percentage of screen width and height.
	FVector2D ClampedScreenPosition = FMath::Clamp(ScreenPosition, FVector2D(0, 0), FVector2D(1, 1));
    
	// Convert it to actual screen pixel coordinates
	FVector2D PixelPosition = FVector2D(ViewportSize.X * ClampedScreenPosition.X, ViewportSize.Y * ClampedScreenPosition.Y);



	
	FVector WorldPosition;
	FVector WorldDirection;

	// Convert screen position to a world space ray
	PlayerController->DeprojectScreenPositionToWorld(PixelPosition.X, PixelPosition.Y, WorldPosition, WorldDirection);
    
	// Set the widget position in front of the camera by a fixed distance, say 200 units.
	FVector NewLocation = WorldPosition + WorldDirection*1000.f; //  + WorldDirection * SpringArm->TargetArmLength*WidgetDistance
	ControlWidgetComp->SetWorldLocation(NewLocation);
	
	// Optionally, make the TalentChooser face the camera.
	FVector CameraLocation = this->GetActorLocation();
	FVector DirectionToCamera = (CameraLocation - NewLocation).GetSafeNormal();
	FRotator NewRotation = DirectionToCamera.Rotation();

	FRotator OffsetRotation(180.0f, 0.0f, 180.0f);  // 90-degree pitch
	FQuat CombinedQuat = NewRotation.Quaternion() * OffsetRotation.Quaternion();
	
	ControlWidgetComp->SetWorldRotation(CombinedQuat.Rotator()); // NewRotation
}*/

void ACameraBase::DeSpawnLoadingWidget()
{
	if (LoadingWidgetComp)
	{
		LoadingWidgetComp->DestroyComponent();
	}
}

void ACameraBase::SpawnLoadingWidget()
{
	FTransform SpellTransform;
	SpellTransform.SetLocation(FVector(500, 0, 0));
	SpellTransform.SetRotation(FQuat(FRotator::ZeroRotator));


	if (LoadingWidgetComp) {
		FRotator NewRotation = LoadingWidgetRotation;
		FQuat QuatRotation = FQuat(NewRotation);
		LoadingWidgetComp->SetRelativeRotation(QuatRotation, false, 0, ETeleportType::None);
		LoadingWidgetComp->SetRelativeLocation(LoadingWidgetLocation);
	}
}


void ACameraBase::SetCameraState(TEnumAsByte<CameraData::CameraState> NewCameraState)
{
	CameraState = NewCameraState;
}

TEnumAsByte<CameraData::CameraState> ACameraBase::GetCameraState()
{
	return CameraState;
}

void ACameraBase::JumpCamera(FHitResult Hit)
{
	FVector ActorLocation = GetActorLocation();

	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw*PI/180);
	const FVector NewPawnLocation = FVector(Hit.Location.X - ActorLocation.Z * 0.7*CosYaw,  Hit.Location.Y - ActorLocation.Z * 0.7*SinYaw, ActorLocation.Z);

	SetActorLocation(NewPawnLocation);
}


void ACameraBase::MoveCamToForward(float DeltaTime, bool Decelerate)
{
	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw*PI/180);
	
	const FVector NewPawnLocation = FVector(0.3*CosYaw,  0.3*SinYaw, 0);

	// Accelerate the camera's speed until it reaches the maximum speed
	if (!Decelerate && CurrentCamSpeed.X < CamSpeed)
	{
		CurrentCamSpeed.X  += AccelerationRate * DeltaTime;
		CurrentCamSpeed.X  = FMath::Min(CurrentCamSpeed.X , CamSpeed);
	}else if(Decelerate && CurrentCamSpeed.X > 0.f)
	{
		CurrentCamSpeed.X  -= DecelerationRate * DeltaTime;
		CurrentCamSpeed.X  = FMath::Min(CurrentCamSpeed.X , CamSpeed);
	}else if(Decelerate)
	{
		CurrentCamSpeed.X = 0.f;
	}


	AddActorWorldOffset(NewPawnLocation * CurrentCamSpeed.X * DeltaTime);
}

void ACameraBase::MoveCamToBackward(float DeltaTime, bool Decelerate)
{
	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw*PI/180);
	
	const FVector NewPawnLocation = FVector(0.3*CosYaw*(-1),  0.3*SinYaw*(-1), 0);

	// Accelerate the camera's speed until it reaches the maximum speed
	if (!Decelerate && CurrentCamSpeed.X > -CamSpeed)
	{
		CurrentCamSpeed.X -= AccelerationRate * DeltaTime;
		CurrentCamSpeed.X = FMath::Min(CurrentCamSpeed.X, CamSpeed);
	}else if(Decelerate && CurrentCamSpeed.X  < 0.f)
	{
		CurrentCamSpeed.X += DecelerationRate * DeltaTime;
		CurrentCamSpeed.X = FMath::Min(CurrentCamSpeed.X, CamSpeed);
	}else if(Decelerate)
	{
		CurrentCamSpeed.X = 0.f;
	}
	
	AddActorWorldOffset(NewPawnLocation * (-1)*CurrentCamSpeed.X * DeltaTime);
	//AddActorWorldOffset(NewPawnLocation * CamSpeed);
}

void ACameraBase::MoveCamToLeft(float DeltaTime, bool Decelerate)
{

	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw*PI/180);
	
	const FVector NewPawnLocation = FVector(0.3*SinYaw,  0.3*CosYaw*(-1), 0);

	// Accelerate the camera's speed until it reaches the maximum speed
	if (!Decelerate && CurrentCamSpeed.Y > -CamSpeed)
	{
		CurrentCamSpeed.Y -= AccelerationRate * DeltaTime;
		CurrentCamSpeed.Y = FMath::Min(CurrentCamSpeed.Y, CamSpeed);
	}else if(Decelerate && CurrentCamSpeed.Y < 0.f)
	{
		CurrentCamSpeed.Y += DecelerationRate * DeltaTime;
		CurrentCamSpeed.Y = FMath::Min(CurrentCamSpeed.Y, CamSpeed);
	}else if(Decelerate)
	{
		CurrentCamSpeed.Y = 0.f;
	}

	AddActorWorldOffset(NewPawnLocation * (-1)*CurrentCamSpeed.Y * DeltaTime);
}

void ACameraBase::MoveCamToRight(float DeltaTime, bool Decelerate)
{
	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw*PI/180);
	
	const FVector NewPawnLocation = FVector(0.3*SinYaw*(-1),  0.3*CosYaw, 0);

	// Accelerate the camera's speed until it reaches the maximum speed
	if (!Decelerate && CurrentCamSpeed.Y < CamSpeed)
	{
		CurrentCamSpeed.Y += AccelerationRate * DeltaTime;
		CurrentCamSpeed.Y = FMath::Min(CurrentCamSpeed.Y, CamSpeed);
	}else if(Decelerate && CurrentCamSpeed.Y > 0.f)
	{
		CurrentCamSpeed.Y -= DecelerationRate * DeltaTime;
		CurrentCamSpeed.Y = FMath::Min(CurrentCamSpeed.Y, CamSpeed);
	}else if(Decelerate)
	{
		CurrentCamSpeed.Y = 0.f;
	}

	AddActorWorldOffset(NewPawnLocation * CurrentCamSpeed.Y * DeltaTime);
}
