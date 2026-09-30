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
#include "NavMesh/NavMeshBoundsVolume.h"
#include "EngineUtils.h"


void ACameraBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACameraBase, BlockControls);
}

// Called when the game starts or when spawned
void ACameraBase::BeginPlay()
{
	Super::BeginPlay();

	if (UseNavBoundMinMax)
	{
		for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
		{
			if (ANavMeshBoundsVolume* NavVolume = *It)
			{
				FVector Origin;
				FVector Extent;
				NavVolume->GetActorBounds(false, Origin, Extent);

				CameraPositionMin = FVector2D(Origin.X - Extent.X, Origin.Y - Extent.Y);
				CameraPositionMax = FVector2D(Origin.X + Extent.X, Origin.Y + Extent.Y);
				break;
			}
		}
	}
	
	ApplyInputMappingContext();

}

void ACameraBase::ApplyInputMappingContext()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	// Get the Enhanced Input Local Player Subsystem from the Local Player related to our Player Controller.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (MappingContext)
		{
			// Safe re-add: remove first (idempotent, no-op if not present) then add so a mid-match
			// (re)possession restores the mapping without clobbering any other contexts the project added.
			Subsystem->RemoveMappingContext(MappingContext);
			Subsystem->AddMappingContext(MappingContext, MappingPriority);
		}
	}
}

void ACameraBase::PawnClientRestart()
{
	Super::PawnClientRestart();
	ApplyInputMappingContext();
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
	TickCameraAngleSmoothing(DeltaTime);
	TickCameraAngleReturn(DeltaTime);
}

void ACameraBase::BeginCameraAngleAdjust()
{
	// Den AKTUELLEN Winkel merken, nicht die Vorgabe aus dem Blueprint: wer seine Kamera vorher
	// bewusst anders gestellt hat, soll genau dorthin zurueckkommen.
	// Laeuft noch eine Rueckfahrt, wird sie abgebrochen - das gemerkte Ziel bleibt aber stehen,
	// sonst wuerde ein schnelles Nachfassen den Ausgangswinkel auf halbem Weg einfrieren.
	if (!bIsAdjustingCameraAngle && !bIsReturningCameraAngle)
	{
		SavedCameraAngle = SpringArmRotator;
	}
	// Das Ziel beginnt dort, wo die Kamera gerade steht - sonst wuerde sie beim Druecken auf ein
	// altes Ziel zuspringen.
	TargetCameraAngle = SpringArmRotator;
	bIsReturningCameraAngle = false;
	bIsAdjustingCameraAngle = true;
}

void ACameraBase::EndCameraAngleAdjust()
{
	if (!bIsAdjustingCameraAngle)
	{
		return;
	}
	bIsAdjustingCameraAngle = false;
	bIsReturningCameraAngle = true;
}

void ACameraBase::AddCameraAngleInput(float PitchInput, float YawInput)
{
	if (!bIsAdjustingCameraAngle || !SpringArm)
	{
		return;
	}

	// Nur das ZIEL bewegen. Den sichtbaren Winkel fuehrt TickCameraAngleSmoothing nach - so
	// schlaegt ein stossweise ankommendes Maus-Delta nicht mehr unmittelbar auf das Bild durch.
	TargetCameraAngle.Pitch = FMath::Clamp(TargetCameraAngle.Pitch + PitchInput * AngleAdjustPitchSpeed,
		AngleAdjustMinPitch, AngleAdjustMaxPitch);
	TargetCameraAngle.Yaw += YawInput * AngleAdjustYawSpeed;

	// Ohne Glaettung sofort durchreichen, damit die Eigenschaft auf 0 wirklich das alte,
	// unmittelbare Verhalten ergibt.
	if (AngleAdjustInterpSpeed <= 0.f)
	{
		SpringArmRotator = TargetCameraAngle;
		SpringArm->SetRelativeRotation(SpringArmRotator);
	}
}

void ACameraBase::TickCameraAngleSmoothing(float DeltaTime)
{
	if (!bIsAdjustingCameraAngle || !SpringArm || AngleAdjustInterpSpeed <= 0.f)
	{
		return;
	}

	// Jede Achse einzeln: FRotator als Ganzes zu interpolieren laeuft beim Yaw ueber die falsche
	// Seite, sobald die Differenz 180 Grad ueberschreitet.
	const FRotator Diff = (TargetCameraAngle - SpringArmRotator).GetNormalized();
	if (FMath::Abs(Diff.Pitch) < 0.01f && FMath::Abs(Diff.Yaw) < 0.01f)
	{
		return;
	}

	const float Alpha = FMath::Clamp(AngleAdjustInterpSpeed * DeltaTime, 0.f, 1.f);
	SpringArmRotator.Pitch += Diff.Pitch * Alpha;
	SpringArmRotator.Yaw   += Diff.Yaw   * Alpha;
	SpringArm->SetRelativeRotation(SpringArmRotator);
}

void ACameraBase::TickCameraAngleReturn(float DeltaTime)
{
	if (!bIsReturningCameraAngle || !SpringArm)
	{
		return;
	}

	// Ueber die kuerzeste Strecke zurueck. FRotator direkt zu interpolieren laeuft beim Yaw ueber
	// die falsche Seite, sobald die Differenz groesser als 180 Grad ist - deshalb die Differenz
	// normalisieren und jede Achse einzeln mit fester Geschwindigkeit fuehren. Feste
	// Geschwindigkeit statt Daempfung, weil eine Daempfung das Ziel nur asymptotisch erreicht und
	// die Rueckfahrt dann nie sauber endet.
	const float Step = FMath::Max(0.f, AngleReturnSpeed * DeltaTime);

	FRotator Diff = (SavedCameraAngle - SpringArmRotator).GetNormalized();
	const float Remaining = FMath::Max3(FMath::Abs(Diff.Pitch), FMath::Abs(Diff.Yaw), FMath::Abs(Diff.Roll));

	if (Remaining <= Step || Remaining < 0.05f)
	{
		SpringArmRotator = SavedCameraAngle;
		bIsReturningCameraAngle = false;
	}
	else
	{
		SpringArmRotator.Pitch += FMath::Clamp(Diff.Pitch, -Step, Step);
		SpringArmRotator.Yaw   += FMath::Clamp(Diff.Yaw,   -Step, Step);
		SpringArmRotator.Roll  += FMath::Clamp(Diff.Roll,  -Step, Step);
	}

	SpringArm->SetRelativeRotation(SpringArmRotator);
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

void ACameraBase::ResetPanVelocity()
{
	CurrentPanVelocity = FVector::ZeroVector;
}

void ACameraBase::PanMoveCameraSmoothed(const FVector& NewPanDirection, float DeltaTime)
{
	// Ohne Anlaufwerte in den alten Weg zurueckfallen. So bleibt das Verhalten fuer alle
	// unveraendert, die die Glaettung nicht wollen.
	if (EdgeScrollAcceleration <= 0.f && EdgeScrollDeceleration <= 0.f)
	{
		PanMoveCamera(NewPanDirection);
		return;
	}

	// Beim Bremsen schneller nachfuehren als beim Beschleunigen: die Kamera soll zuegig stehen,
	// wenn die Maus die Kante verlaesst.
	const bool  bIsBraking = NewPanDirection.IsNearlyZero();
	const float Rate  = bIsBraking ? EdgeScrollDeceleration : EdgeScrollAcceleration;
	const float Alpha = (Rate <= 0.f) ? 1.f : FMath::Clamp(Rate * DeltaTime, 0.f, 1.f);

	CurrentPanVelocity += (NewPanDirection - CurrentPanVelocity) * Alpha;

	// Reste abschneiden, sonst kriecht die Kamera durch die exponentielle Annaeherung ewig weiter.
	if (bIsBraking && CurrentPanVelocity.SizeSquared() < 1.f)
	{
		CurrentPanVelocity = FVector::ZeroVector;
		return;
	}

	// Auf 60 Bilder je Sekunde normiert: der alte Pfad rechnete ohne DeltaTime, war also
	// bildratenabhaengig. Der Faktor haelt die gewohnte Geschwindigkeit bei 60 fps und macht sie
	// darunter und darueber gleich schnell.
	AddActorWorldOffset(CurrentPanVelocity * GetActorLocation().Z * 0.001 * DeltaTime * 60.f);
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

	// Solange die Winkelverstellung laeuft, gehen BEIDE Achsen ueber AddCameraAngleInput.
	// Der alte Pfad darunter konnte den Pitch faktisch nie bewegen: RotateSpringArmPitchFree
	// verlangt fuers Hochkippen zusaetzlich TargetArmLength < SpringArmStartRotator und fuers
	// Absenken Pitch >= SpringArmMaxRotator. Im normalen Zoombereich ist keine der beiden
	// Bedingungen erfuellt, also blieb nur das Drehen nach links und rechts uebrig.
	if (bIsAdjustingCameraAngle)
	{
		// Kleine Totzone gegen Zittern - aber keine 100 Pixel wie im alten Pfad, sonst laesst
		// sich der Winkel nicht feinfuehlig setzen.
		const float AngleDeadZone = 1.f;
		const float YawInput   = FMath::Abs(Delta.X) > AngleDeadZone ? Delta.X : 0.f;
		const float PitchRaw   = FMath::Abs(Delta.Y) > AngleDeadZone ? Delta.Y : 0.f;
		// Grundrichtung: Maus nach oben hebt den Blick an. Das Vorzeichen steht hier und nicht in
		// AddCameraAngleInput, damit die Tastenbelegung nicht die Winkelrechnung mitdreht.
		const float PitchInput = bInvertCameraAnglePitch ? PitchRaw : -PitchRaw;

		AddCameraAngleInput(PitchInput, YawInput);

		// Basislinie auf die AKTUELLE Mausposition. Der alte Pfad schob sie nur um einen
		// normalisierten Schritt weiter; dadurch blieb das Delta gross und die Kamera drehte
		// weiter, obwohl die Maus stand.
		PreviousMouseLocation = MouseLocation;
		return !Delta.IsNearlyZero();
	}
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


float ACameraBase::GetFrameScale() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.f;
	}
	// Auf 60 FPS normiert, damit alle bereits eingestellten Kamerawerte ihre Wirkung behalten.
	// Deckel bei 3 (= 20 FPS): ein Ladehitch soll die Kamera nicht wegkatapultieren.
	return FMath::Clamp(World->GetDeltaSeconds() * 60.f, 0.f, 3.f);
}

bool ACameraBase::RotateCamera(float Direction, float Add, bool stopCam)
{
	// Direction: 1.0f = Left, -1.0f = Right

	// Alles hier lief pro FRAME statt pro Sekunde: Rampe und angewandter Yaw haengen damit
	// an der Bildrate, Q/E dreht auf einer schnellen Maschine deutlich schneller als auf
	// einer langsamen. GetFrameScale() normiert auf 60 FPS, die eingestellten Werte
	// (RotationIncreaser, AddCamRotation) behalten dadurch exakt ihre bisherige Wirkung.
	const float FrameScale = GetFrameScale();

	if(stopCam && CurrentRotationValue > 0.f)
	{
		if(FMath::IsNearlyEqual(CurrentRotationValue, 0.f, RotationIncreaser*3*FrameScale))
			CurrentRotationValue = 0.0000f;
		else
			CurrentRotationValue -= RotationIncreaser*3*FrameScale;
	}
	else if(CurrentRotationValue < Add)
	{
		CurrentRotationValue += RotationIncreaser*FrameScale;
	}

	// Apply rotation based on direction
	SpringArmRotator.Yaw += CurrentRotationValue * Direction * FrameScale;

	// Das fruehere "Yaw auf ganze Grad runden, solange schnell gedreht wird" ist entfernt:
	// bei hoher Bildrate ist der Yaw-Zuwachs pro Frame kleiner als 1 Grad, das Runden hat
	// ihn dann komplett verschluckt und die Drehung in 1-Grad-Stufen springen lassen -
	// genau das sichtbare Ruckeln. Die Fmod-Normierung unten haelt den Wert ohnehin im Rahmen.

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

bool ACameraBase::RotateCamYawTowards(float TargetYaw, float InterpSpeed, float DeltaTime, float Toleranz)
{
	if (InterpSpeed <= 0.f || DeltaTime <= 0.f)
	{
		return false;
	}

	// UnwindDegrees liefert die Differenz im Bereich [-180, 180] - damit nimmt die Kamera
	// immer den kuerzeren Weg und dreht nicht einmal komplett herum, wenn Ziel und Ist
	// beiderseits des 0/360-Sprungs liegen.
	const float Differenz = FMath::UnwindDegrees(TargetYaw - static_cast<float>(SpringArmRotator.Yaw));

	if (FMath::Abs(Differenz) <= Toleranz)
	{
		return true;
	}

	// Anteil der Restdifferenz, der in diesem Takt abgebaut wird. Die Begrenzung auf 1
	// verhindert ein Ueberschwingen bei grossen DeltaTime-Werten (Ladepausen, Haltepunkte).
	const float Anteil = FMath::Clamp(InterpSpeed * DeltaTime, 0.f, 1.f);
	SpringArmRotator.Yaw += Differenz * Anteil;

	// Auf [0, 360) normieren - dieselbe Konvention wie in RotateCamera.
	if (SpringArmRotator.Yaw >= 360.f)
		SpringArmRotator.Yaw = FMath::Fmod(SpringArmRotator.Yaw, 360.f);
	if (SpringArmRotator.Yaw < 0.f)
		SpringArmRotator.Yaw = 360.f + FMath::Fmod(SpringArmRotator.Yaw, 360.f);

	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(SpringArmRotator);
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
		// war ein Frame-Schritt: bei hoher Bildrate zoomte die AutoCam entsprechend schneller
		SpringArm->TargetArmLength += (AutoZoomSpeed/10) * GetFrameScale();

		return false;
	}

	return true;
}

bool ACameraBase::ZoomOutToPosition(float Distance, const FVector SelectedActorPosition)
{


	if (SpringArm && SpringArm->TargetArmLength - SelectedActorPosition.Z < Distance)
	{
			SpringArm->TargetArmLength += FastZoomSpeed * GetFrameScale();

		return false;
	}

	return true;
}

bool ACameraBase::ZoomInToPosition(float Distance, const FVector SelectedActorPosition)
{

	if (SpringArm && SpringArm->TargetArmLength > 100.f && SpringArm->TargetArmLength - SelectedActorPosition.Z > Distance)
	{

		// Reinzoomen war der haerteste Frame-Schritt von allen (250 pro Frame): bei 200 FPS
		// sind das 50000 Einheiten pro Sekunde, deshalb wirkte es ruckartig statt weich.
		SpringArm->TargetArmLength -= FastZoomSpeed * GetFrameScale();

		return false;
	}
	return true;
}

void ACameraBase::LockOnUnit(AUnitBase* Unit)
{
	if (Unit && Unit->GetUnitState() != UnitData::Dead) {
		FVector ActorLocation = Unit->GetMassActorLocation();

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
	const bool bHasInput = !Direction.IsNearlyZero();

	// CurrentCamSpeed.X/Y carries the world-space pan velocity between frames (Z belongs to the zoom).
	FVector PanVelocity(CurrentCamSpeed.X, CurrentCamSpeed.Y, 0.f);

	if (!bHasInput && PanVelocity.IsNearlyZero())
	{
		CurrentCamSpeed.X = 0.f;
		CurrentCamSpeed.Y = 0.f;
		return;
	}

	FVector DesiredVelocity = FVector::ZeroVector;
	if (bHasInput)
	{
		// Normalize the direction
		Direction.Normalize();

		// Die Blickrichtung ist Pawn-Drehung PLUS SpringArm-Drehung - der SpringArm sitzt
		// relativ am Aktor. Frueher ging hier nur der relative Anteil ein; sobald ein
		// PlayerStart gedreht war (die Xeno-Starts stehen auf Yaw 180), zeigte die Ansicht
		// in die eine und die Bewegung in die andere Richtung - die Steuerung fuehlte sich
		// invertiert an. Mit der Weltdrehung stimmt sie auf jeder Karte.
		const float WorldYaw = GetActorRotation().Yaw + SpringArmRotator.Yaw;
		const float CosYaw = FMath::Cos(WorldYaw * PI / 180.f);
		const float SinYaw = FMath::Sin(WorldYaw * PI / 180.f);

		// Transform the input direction based on camera rotation
		// Forward/Backward uses Cos/Sin, Left/Right uses Sin/Cos with appropriate signs
		FVector WorldDirection;
		WorldDirection.X = Direction.X * CosYaw - Direction.Y * SinYaw;
		WorldDirection.Y = Direction.X * SinYaw + Direction.Y * CosYaw;
		WorldDirection.Z = 0.f;

		DesiredVelocity = WorldDirection * CamSpeed;
	}

	// Ramp towards the desired velocity (units/s^2): AccelerationRate while a key is held,
	// DecelerationRate once the input is released and the camera coasts to a stop.
	const float RampRate = bHasInput ? AccelerationRate : DecelerationRate;
	PanVelocity = FMath::VInterpConstantTo(PanVelocity, DesiredVelocity, DeltaTime, RampRate);
	CurrentCamSpeed.X = PanVelocity.X;
	CurrentCamSpeed.Y = PanVelocity.Y;

	if (PanVelocity.IsNearlyZero())
	{
		return;
	}

	FVector ProposedLocation = GetActorLocation() + (PanVelocity * DeltaTime);

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
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->IsA(ALandscape::StaticClass()))
		{
			if (ProposedLocation.Z < HitResult.Location.Z)
			{
				float OldZ = ProposedLocation.Z;
				ProposedLocation.Z = HitResult.Location.Z + 10.f;
			}
		}
	}

	// Apply boundary clamping if limits are set
	if (CameraPositionMin.X != 0.f || CameraPositionMax.X != 0.f)
	{
		FVector BeforeClamp = ProposedLocation;
		ProposedLocation.X = FMath::Clamp(ProposedLocation.X, CameraPositionMin.X, CameraPositionMax.X);
		ProposedLocation.Y = FMath::Clamp(ProposedLocation.Y, CameraPositionMin.Y, CameraPositionMax.Y);
	}
	
	SetActorLocation(ProposedLocation);
}


