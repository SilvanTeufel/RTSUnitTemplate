// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Camera/CameraBase.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Controller/Input/EnhancedInputComponentBase.h"
#include "Controller/Input/GameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Landscape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UnrealEngine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"


void ACameraBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

// Called when the game starts or when spawned
void ACameraBase::BeginPlay()
{
	Super::BeginPlay();
	
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
}

void ACameraBase::CreateCameraComp()
{
	RootScene = RootComponent;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCameraBaseCapsule()); // RootScene // GetCapsuleComponent()
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->SetRelativeRotation(SpringArmRotator);
	SpringArm->SetIsReplicated(true);

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
	if(!SpringArm) return;

	if(!Invert && SpringArm->TargetArmLength < SpringArmStartRotator)
	{
		// Zoom In: Rotiere nach oben (Pitch wird größer)

		// Prüfe ob wir bereits am oder über dem Limit sind
		if(SpringArmRotator.Pitch >= SpringArmMinRotator)
		{
			SpringArmRotator.Pitch = SpringArmMinRotator;
			SpringArmRotatorSpeed = 0.f;
			SpringArm->SetRelativeRotation(SpringArmRotator);
			return;
		}

		// Initialisiere Speed falls negativ
		if(SpringArmRotatorSpeed <= 0.f)
			SpringArmRotatorSpeed = 0.f;	

		// Beschleunige
		if(SpringArmRotatorSpeed < SpringArmRotatorMaxSpeed)
			SpringArmRotatorSpeed += SpringArmRotatorAcceleration;

		float NewPitch = SpringArmRotator.Pitch + SpringArmRotatorSpeed;

		// Begrenze sanft auf das Minimum
		if(NewPitch >= SpringArmMinRotator)
		{
			SpringArmRotator.Pitch = SpringArmMinRotator;
			SpringArmRotatorSpeed = 0.f;
		}
		else
		{
			SpringArmRotator.Pitch = NewPitch;
		}

		SpringArm->SetRelativeRotation(SpringArmRotator);
	}
	else if(Invert)
	{
		// Zoom Out: Rotiere nach unten (Pitch wird kleiner)

		// Prüfe ob wir bereits am oder unter dem Limit sind
		if(SpringArmRotator.Pitch <= SpringArmMaxRotator)
		{
			SpringArmRotator.Pitch = SpringArmMaxRotator;
			SpringArmRotatorSpeed = 0.f;
			SpringArm->SetRelativeRotation(SpringArmRotator);
			return;
		}

		// Initialisiere Speed falls positiv
		if(SpringArmRotatorSpeed >= 0.f)
			SpringArmRotatorSpeed = 0.f;	

		// Beschleunige (negativ)
		if(SpringArmRotatorSpeed > -SpringArmRotatorMaxSpeed)
			SpringArmRotatorSpeed -= SpringArmRotatorAcceleration;

		float NewPitch = SpringArmRotator.Pitch + SpringArmRotatorSpeed;

		// Begrenze sanft auf das Maximum
		if(NewPitch <= SpringArmMaxRotator)
		{
			SpringArmRotator.Pitch = SpringArmMaxRotator;
			SpringArmRotatorSpeed = 0.f;
		}
		else
		{
			SpringArmRotator.Pitch = NewPitch;
		}

		SpringArm->SetRelativeRotation(SpringArmRotator);
	}
	else
	{
		// Stoppe die Rotation
		SpringArmRotatorSpeed = 0.f;
		SpringArm->SetRelativeRotation(SpringArmRotator);
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


bool ACameraBase::RotateCamera(float Direction, float Add, bool stopCam)
{
	// Direction: 1.0f = Left, -1.0f = Right

	if(stopCam && CurrentRotationValue > 0.f)
	{
		if(FMath::IsNearlyEqual(CurrentRotationValue, 0.f, RotationIncreaser*3))
			CurrentRotationValue = 0.0000f;
		else
			CurrentRotationValue -= RotationIncreaser*3;
	}
	else if(CurrentRotationValue < Add)
	{
		CurrentRotationValue += RotationIncreaser;
	}

	// Apply rotation based on direction
	SpringArmRotator.Yaw += CurrentRotationValue * Direction;

	if(CurrentRotationValue >= 1.0f)
		SpringArmRotator.Yaw = floor(SpringArmRotator.Yaw+0.5);

	// Normalize Yaw to [0, 360)
	if (SpringArmRotator.Yaw >= 360.f) 
		SpringArmRotator.Yaw = FMath::Fmod(SpringArmRotator.Yaw, 360.f);
	if (SpringArmRotator.Yaw < 0.f) 
		SpringArmRotator.Yaw = 360.f + FMath::Fmod(SpringArmRotator.Yaw, 360.f);

	if(SpringArm)
		SpringArm->SetRelativeRotation(SpringArmRotator);

	// Check if we've reached a camera angle
	if (FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[0], RotationIncreaser) ||
		FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[1], RotationIncreaser) ||
		FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[2], RotationIncreaser) ||
		FMath::IsNearlyEqual(SpringArmRotator.Yaw, CameraAngles[3], RotationIncreaser))
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
	if (ControlWidget)
		ControlWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ACameraBase::ShowControlWidget()
{
	if (ControlWidget)
	{
		ControlWidget->SetVisibility(ESlateVisibility::Visible);
	}
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

void ACameraBase::JumpCamera(FHitResult Hit)
{
	FVector ActorLocation = GetActorLocation();

	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw*PI/180);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw*PI/180);
	const FVector NewPawnLocation = FVector(Hit.Location.X - ActorLocation.Z * 0.7*CosYaw,  Hit.Location.Y - ActorLocation.Z * 0.7*SinYaw, ActorLocation.Z);

	SetActorLocation(NewPawnLocation);
}

void ACameraBase::MoveInDirection(FVector Direction, float DeltaTime)
{
	FString NetModeStr = TEXT("UNKNOWN");
	if (GetWorld())
	{
		switch (GetWorld()->GetNetMode())
		{
			case NM_Standalone: NetModeStr = TEXT("Standalone"); break;
			case NM_DedicatedServer: NetModeStr = TEXT("DedicatedServer"); break;
			case NM_ListenServer: NetModeStr = TEXT("ListenServer"); break;
			case NM_Client: NetModeStr = TEXT("Client"); break;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("=== MoveInDirection START [%s] ==="), *NetModeStr);
	UE_LOG(LogTemp, Warning, TEXT("  Input Direction: %s, DeltaTime: %f"), *Direction.ToString(), DeltaTime);
	UE_LOG(LogTemp, Warning, TEXT("  Current Location: %s"), *GetActorLocation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("  CamSpeed: %f"), CamSpeed);

	if (Direction.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("  Direction is nearly zero, returning early"));
		return;
	}

	// Normalize the direction
	Direction.Normalize();
	UE_LOG(LogTemp, Warning, TEXT("  Normalized Direction: %s"), *Direction.ToString());

	// Calculate the movement direction relative to the SpringArm rotation
	const float CosYaw = FMath::Cos(SpringArmRotator.Yaw * PI / 180.f);
	const float SinYaw = FMath::Sin(SpringArmRotator.Yaw * PI / 180.f);

	UE_LOG(LogTemp, Warning, TEXT("  SpringArmRotator.Yaw: %f, CosYaw: %f, SinYaw: %f"), SpringArmRotator.Yaw, CosYaw, SinYaw);

	// Transform the input direction based on camera rotation
	// Forward/Backward uses Cos/Sin, Left/Right uses Sin/Cos with appropriate signs
	FVector WorldDirection;
	WorldDirection.X = Direction.X * CosYaw - Direction.Y * SinYaw;
	WorldDirection.Y = Direction.X * SinYaw + Direction.Y * CosYaw;
	WorldDirection.Z = 0.f;

	UE_LOG(LogTemp, Warning, TEXT("  WorldDirection: %s"), *WorldDirection.ToString());

	// Always use direct location manipulation since AddMovementInput doesn't work for camera
	UE_LOG(LogTemp, Warning, TEXT("  Using direct location manipulation"));

	FVector ProposedLocation = GetActorLocation() + (WorldDirection * CamSpeed * DeltaTime);
	UE_LOG(LogTemp, Warning, TEXT("  Proposed Location (before trace): %s"), *ProposedLocation.ToString());

	// Perform line trace for Z adjustment
	const float TraceVerticalRange = 3000.f;
	const FVector TraceStart = ProposedLocation + FVector(0, 0, TraceVerticalRange);
	const FVector TraceEnd = ProposedLocation - FVector(0, 0, TraceVerticalRange);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams);
	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Line trace HIT at: %s"), *HitResult.Location.ToString());
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->IsA(ALandscape::StaticClass()))
		{
			if (ProposedLocation.Z < HitResult.Location.Z)
			{
				float OldZ = ProposedLocation.Z;
				ProposedLocation.Z = HitResult.Location.Z + 10.f;
				UE_LOG(LogTemp, Warning, TEXT("  Adjusted Z from %f to %f"), OldZ, ProposedLocation.Z);
			}
		}
	}

	// Apply boundary clamping if limits are set
	if (CameraPositionMin.X != 0.f || CameraPositionMax.X != 0.f)
	{
		FVector BeforeClamp = ProposedLocation;
		ProposedLocation.X = FMath::Clamp(ProposedLocation.X, CameraPositionMin.X, CameraPositionMax.X);
		ProposedLocation.Y = FMath::Clamp(ProposedLocation.Y, CameraPositionMin.Y, CameraPositionMax.Y);
		UE_LOG(LogTemp, Warning, TEXT("  Clamped location from %s to %s"), *BeforeClamp.ToString(), *ProposedLocation.ToString());
	}

	UE_LOG(LogTemp, Warning, TEXT("  Setting Actor Location to: %s"), *ProposedLocation.ToString());
	SetActorLocation(ProposedLocation);

	UE_LOG(LogTemp, Warning, TEXT("  Final Location: %s"), *GetActorLocation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("=== MoveInDirection END ==="));
}


