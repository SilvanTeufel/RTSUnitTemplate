// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Characters/CameraBase.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Controller/Input/EnhancedInputComponentBase.h"
#include "Controller/Input/GameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "Controller/CameraControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UnrealEngine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetMathLibrary.h"

ACameraBase::ACameraBase(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	if (RootComponent == nullptr) {
		RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	}
	
	CreateCameraComp();

	ControlWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("ControlWidget"));
	ControlWidgetComp->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);
	
		GetCameraBaseCapsule()->BodyInstance.bLockXRotation = true;
		GetCameraBaseCapsule()->BodyInstance.bLockYRotation = true;
		GetCameraBaseCapsule()->BodyInstance.bLockZRotation = true;
	
		GetMesh()->AttachToComponent(GetCameraBaseCapsule(), FAttachmentTransformRules::KeepRelativeTransform);


		UCapsuleComponent* CComponent = GetCapsuleComponent();
		if (CComponent)
		{
			CComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);  // Enable both physics and overlap query
			CComponent->SetCollisionResponseToAllChannels(ECR_Ignore);  // Start by ignoring all channels
			CComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);  // Block other pawns (this can be adjusted based on your requirements)
			CComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // Important: Block WorldStatic so it can walk on static objects like ground, walls, etc.
			CComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);  // ECR_Overlap Overlap with dynamic objects (adjust as needed)
		}

	
		UMeshComponent* CMesh = GetMesh();
		if(CMesh)
		{
	
			CMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // Typically, we use the capsule for physics and mesh for simple queries like overlap
			CMesh->SetCollisionResponseToAllChannels(ECR_Ignore);  // Start by ignoring all channels
			CMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);  // Overlap with other pawns
			CMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);  // Overlap with dynamic objects
		}

}

// Called when the game starts or when spawned
void ACameraBase::BeginPlay()
{
	Super::BeginPlay();

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

	//if(ShowControlWidgetAtStart) ShowControlWidget();
}

/*
void ACameraBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	UEnhancedInputComponentBase* EnhancedInputComponentBase = Cast<UEnhancedInputComponentBase>(PlayerInputComponent);

	if(EnhancedInputComponentBase)
	{
		check(EnhancedInputComponentBase);
		const FGameplayTags& GameplayTags = FGameplayTags::Get();

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Tab_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_Tab_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Tab_Released, ETriggerEvent::Triggered, this, &ACameraBase::Input_Tab_Released, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_LeftClick_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_LeftClick_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_LeftClick_Released, ETriggerEvent::Triggered, this, &ACameraBase::Input_LeftClick_Released, 0);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_RightClick_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_RightClick_Pressed, 0);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_G_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_G_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_A_Pressed, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Shift_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_Shift_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Shift_Released, ETriggerEvent::Triggered, this, &ACameraBase::Input_Shift_Released, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Ctrl_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::Input_Ctrl_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Ctrl_Released, ETriggerEvent::Triggered, this, &ACameraBase::Input_Ctrl_Released, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Space_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 7);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Space_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 8);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_W_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 1);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_W_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 111);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_S_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 2);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_S_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 222);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 3);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 333);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_D_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 4);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_D_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 444);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_X_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 5);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_X_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 555);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Y_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 6);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Y_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 666);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Q_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 9);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_E_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 10);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Q_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 999);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_E_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 101010);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_T_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 12);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_P_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 14);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_O_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 15);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D1, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 13);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D2, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 13);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Middle_Mouse_Pressed, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 16);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Middle_Mouse_Released, ETriggerEvent::Triggered, this, &ACameraBase::SwitchControllerStateMachine, 17);

		
	}

	
}
*/
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

	SetControlWidgetLocation();
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

	SetControlWidgetLocation();
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

bool ACameraBase::RotateFree(FVector MouseLocation)
{
    const float RotationThreshold = 50.f; // Ersetzen Sie dies durch den gewünschten Schwellenwert.

	// Viewport-Größe holen
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Den Mittelpunkt des Bildschirms berechnen
	FVector2D ScreenCenter = ViewportSize / 2;

	float XDifference = ScreenCenter.X -  MouseLocation.X;
	float YDifference = MouseLocation.Y - ScreenCenter.Y;
	
    SpringArmRotator = SpringArm->GetRelativeRotation();
    bool bRotated = false;

    // Prüfen, ob die X-Differenz größer als der Schwellenwert ist und Yaw drehen.
    if (FMath::Abs(XDifference) > RotationThreshold)
    {
    	float TempYaw = SpringArmRotator.Yaw;
    	AdjustSpringArmRotation(XDifference, TempYaw);
    	SpringArmRotator.Yaw = TempYaw;
        bRotated = true;
    }
	
    // Prüfen, ob die Y-Differenz größer als der Schwellenwert ist und Pitch drehen.
    if (FMath::Abs(YDifference) > RotationThreshold)
    {
    	float TempPitch = SpringArmRotator.Pitch;
    	AdjustSpringArmRotation(YDifference, TempPitch);
    	SpringArmRotator.Pitch = TempPitch;
        bRotated = true;
    }
	
    // Aktualisieren der SpringArm Rotation.
    SpringArm->SetRelativeRotation(SpringArmRotator);

    // Wenn der Yaw über 360 geht, setze ihn zurück auf 0.
    if (SpringArmRotator.Yaw >= 360) SpringArmRotator.Yaw = 0.f;

    // Gibt zurück, ob eine Drehung durchgeführt wurde oder nicht.
    return bRotated;
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

	SetControlWidgetLocation();
	
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

	SetControlWidgetLocation();
	
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
		SetControlWidgetLocation();
	}
}

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
}

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
/*
void ACameraBase::Input_LeftClick_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->LeftClickPressed();
		CameraControllerBase->JumpCamera();
	}
}

void ACameraBase::Input_LeftClick_Released(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->LeftClickReleased();
	}
}

void ACameraBase::Input_RightClick_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->RightClickPressed();
	}
}

void ACameraBase::Input_G_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->ToggleLockCamToCharacter();
	}
}

void ACameraBase::Input_A_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;

	
	//ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	//if(CameraControllerBase)
	//{
		//CameraControllerBase->TPressed();
	//}
}

void ACameraBase::Input_Ctrl_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->IsStrgPressed = true;
	}
}

void ACameraBase::Input_Ctrl_Released(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->IsStrgPressed = false;
	}
}

void ACameraBase::Input_Tab_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	ShowControlWidget();
}

void ACameraBase::Input_Tab_Released(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	HideControlWidget();
}

void ACameraBase::Input_Shift_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	
	if(CameraControllerBase)
	{
		CameraControllerBase->ShiftPressed();
	}
}

void ACameraBase::Input_Shift_Released(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	
	if(CameraControllerBase)
	{
		CameraControllerBase->ShiftReleased();
	}
}
*/
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
	//AddActorWorldOffset(NewPawnLocation * CamSpeed);
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
/*
void ACameraBase::SwitchControllerStateMachine(const FInputActionValue& InputActionValue, int32 NewCameraState)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	
	if(CameraControllerBase)
	{
		if(CameraControllerBase->IsStrgPressed)
			switch (NewCameraState)
			{
		case 0:
			{

			}
				break;
		case 1:
			{
				CameraControllerBase->WIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 111:
			{
				CameraControllerBase->WIsPressedState = 2;
			} break;
		case 2:
			{
				CameraControllerBase->SIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 222:
			{
				CameraControllerBase->SIsPressedState = 2;
				//CameraControllerBase->SWasPressed = true;
			} break;
		case 3:
			{
				CameraControllerBase->AIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
				//SetCameraState(CameraData::MoveLeft);
			} break;
		case 333:
			{
				CameraControllerBase->AIsPressedState = 2;
				//CameraControllerBase->AWasPressed = true;
			} break;
		case 4:
			{
				CameraControllerBase->DIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
				//SetCameraState(CameraData::MoveRight);
			} break;
		case 444:
			{
				CameraControllerBase->DIsPressedState = 2;
				//CameraControllerBase->DWasPressed = true;
			} break;
		case 5:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingInState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingInState = 1;
					SetCameraState(CameraData::ZoomIn);
				}
			}
				break;
		case 555:
			{
					CameraControllerBase->CamIsZoomingInState = 2;
			}
			break;
		case 6:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingOutState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingOutState = 1;
					SetCameraState(CameraData::ZoomOut);
				}
			}
			break;
		case 666:
			{
				CameraControllerBase->CamIsZoomingOutState = 2;
			}
			break;
		case 7: SetCameraState(CameraData::ZoomOutPosition); break;
		case 8: SetCameraState(CameraData::ZoomInPosition); break;
		case 9:
			{
				if(CameraControllerBase->LockCameraToCharacter)
					return;
				
				SetCameraState(CameraData::RotateLeft);
			} break;
		case 10:
			{
				if(CameraControllerBase->LockCameraToCharacter)
					return;
				
				SetCameraState(CameraData::RotateRight);
			} break;
		case 11: SetCameraState(CameraData::LockOnCharacter); break;
		case 12: SetCameraState(CameraData::ZoomToThirdPerson); break;
		case 14:
			{	SetCameraState(CameraData::OrbitAndMove); break;
				//CameraControllerBase->OrbitAtLocation(FVector(1000.f, -1000.f, 500.f), 0.033f);
			} break;
		case 15:
			{

				CameraControllerBase->SpawnMissileRain(4, FVector(1000.f, -1000.f, 1000.f));

				CameraControllerBase->SpawnEffectArea(3, FVector(1000.f, -1000.f, 10.f));
			} break;
		case 16:
			{
				CameraControllerBase->MiddleMouseIsPressed = true;
			} break;
		case 17:
			{
				CameraControllerBase->MiddleMouseIsPressed = false;
			} break;
		default:
			{
				//SetCameraState(CameraData::UseScreenEdges);
			}break;
		}

		if(!CameraControllerBase->IsStrgPressed)
		switch (NewCameraState)
		{
		case 1:
			{
				CameraControllerBase->WIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 111:
			{
				CameraControllerBase->WIsPressedState = 2;
			} break;
		case 2:
			{
				CameraControllerBase->SIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 222:
			{
				CameraControllerBase->SIsPressedState = 2;
			} break;
		case 3:
			{
				CameraControllerBase->AIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 333:
			{
				CameraControllerBase->AIsPressedState = 2;
			} break;
		case 4:
			{
				CameraControllerBase->DIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 444:
			{
				CameraControllerBase->DIsPressedState = 2;
			} break;
		case 5:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingInState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingInState = 1;
					SetCameraState(CameraData::ZoomIn);
				}
			}
				break;
		case 555:
			{
					CameraControllerBase->CamIsZoomingInState = 2;
			}
			break;
		case 6:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingOutState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingOutState = 1;
					SetCameraState(CameraData::ZoomOut);
				}
			}
			break;
		case 666:
			{
				CameraControllerBase->CamIsZoomingOutState = 2;
			}
			break;
		case 8: SetCameraState(CameraData::ZoomInPosition); break;
		case 9:
			{
				CameraControllerBase->CamIsRotatingLeft = true;
				
				if(CameraControllerBase->LockCameraToCharacter ||
					CameraControllerBase->WIsPressedState ||
					CameraControllerBase->AIsPressedState ||
					CameraControllerBase->SIsPressedState ||
					CameraControllerBase->DIsPressedState)
					return;
				
				SetCameraState(CameraData::HoldRotateLeft);
			} break;
		case 10:
			{
				CameraControllerBase->CamIsRotatingRight = true;
				
				if(CameraControllerBase->LockCameraToCharacter ||
					CameraControllerBase->WIsPressedState ||
					CameraControllerBase->AIsPressedState ||
					CameraControllerBase->SIsPressedState ||
					CameraControllerBase->DIsPressedState)
					return;
				
				SetCameraState(CameraData::HoldRotateRight);
			} break;
		case 999:
			{
				CameraControllerBase->CamIsRotatingLeft = false;
			}
			break;
		case 101010:
			{
				CameraControllerBase->CamIsRotatingRight = false;
			}
			break;
		case 12:
			{
				CameraControllerBase->TPressed();
			}
			break;
		case 13:
			{
				float FloatValue = InputActionValue.Get<float>();

				if(CameraControllerBase->ScrollZoomCount <= 10.f)
				CameraControllerBase->ScrollZoomCount += FloatValue*2;
				
				if(CameraControllerBase->LockCameraToCharacter)
					return;
				
				if(FloatValue > 0)
				{
					SetCameraState(CameraData::ScrollZoomIn);
				}
				else
				{
					SetCameraState(CameraData::ScrollZoomOut);
				}
			}
			break;
		case 16:
			{
				CameraControllerBase->MiddleMouseIsPressed = true;
			} break;
		case 17:
			{
				CameraControllerBase->MiddleMouseIsPressed = false;
			} break;
		}
	}

}
*/