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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UCapsuleComponent* GetCameraBaseCapsule() const {
		return GetCapsuleComponent();
	}

	FVector PreviousMouseLocation;
	bool bLockCameraZRotation = true;


	// In your ACameraBase.h within the class declaration:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Limits")
	FVector2D CameraPositionMin = FVector2D(-10000.0f, -10000.0f); // Example minimum limits for X and Y

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Limits")
	FVector2D CameraPositionMax = FVector2D(10000.0f, 10000.0f); 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Limits")
	bool UseNavBoundMinMax = true;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PawnClientRestart() override;

	/** (Re)adds MappingContext to the owning local player's Enhanced Input subsystem. Idempotent. */
	virtual void ApplyInputMappingContext();

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputConfig* InputConfig;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool BlockControls = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool TabToggled = false;
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

	// ---- Winkel auf gedrueckte Taste, danach zurueck -------------------------------------
	//
	// Gedacht fuer ein kurzes Hineinschauen: Taste halten, Winkel frei verstellen, loslassen -
	// und die Kamera findet von selbst in ihren Ausgangswinkel zurueck. Der Ausgangswinkel wird
	// beim DRUECKEN gemerkt, nicht fest verdrahtet: wer seine Kamera vorher dauerhaft anders
	// eingestellt hat, bekommt seinen eigenen Winkel zurueck und nicht den der Vorgabe.

	/** Laeuft die Rueckfahrt gerade? Nur lesen. */
	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bIsReturningCameraAngle = false;

	/** Wird die Taste gerade gehalten? Nur lesen. */
	UPROPERTY(BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bIsAdjustingCameraAngle = false;

	/** Pitch-Grad je Mauseinheit waehrend des Haltens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AngleAdjustPitchSpeed = 0.35f;

	/** Yaw-Grad je Mauseinheit waehrend des Haltens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AngleAdjustYawSpeed = 0.35f;

	/** Grenzen fuer den Pitch waehrend des Verstellens, damit die Kamera nicht durchkippt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AngleAdjustMinPitch = -85.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AngleAdjustMaxPitch = -5.f;

	/**
	 * Dreht die Richtung der Hochachse beim Verstellen (mittlere Maustaste gehalten).
	 *
	 * Aus (Vorgabe): Maus nach OBEN hebt den Blick an, Maus nach unten senkt ihn.
	 * An: genau umgekehrt.
	 *
	 * Als Schalter und nicht fest im Code, weil sich die Vorliebe ohne neuen Build umstellen
	 * laesst - ein Build verlangt jedes Mal, den Editor zu schliessen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bInvertCameraAnglePitch = false;

	/** Tempo der Rueckfahrt in Grad je Sekunde. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AngleReturnSpeed = 180.f;

	/**
	 * Wie schnell der Winkel dem Mausziel folgt (exponentiell, Einheit 1/s).
	 *
	 * Das Ruckeln kam daher, dass jedes Maus-Delta sofort und ungefiltert im Winkel landete -
	 * Maus-Deltas kommen aber stossweise. Jetzt wandert nur ein ZIEL mit der Maus, und der
	 * sichtbare Winkel laeuft dem Ziel hinterher. Exponentiell, weil das von selbst weich
	 * anfaengt und weich auslaeuft, ohne eine eigene Beschleunigungsrechnung.
	 *
	 * Groesser = direkter und haerter, kleiner = weicher und traeger. Unter etwa 8 faengt es an,
	 * sich schwammig anzufuehlen; 0 schaltet die Glaettung ganz ab.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AngleAdjustInterpSpeed = 14.f;

	/**
	 * Faehrt der Winkel beim Loslassen der Taste in den Ausgangswinkel zurueck?
	 *
	 * An die mittlere Maustaste gehaengt, weil das freie Drehen dort bereits liegt (RotateFree) -
	 * so braucht es keine neue Eingabeaktion. Aus bedeutet: freies Drehen bleibt eine dauerhafte
	 * Verstellung, wie bisher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bReturnCameraAngleOnRelease = true;

	/** Taste gedrueckt: merkt den aktuellen Winkel und gibt das Verstellen frei. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void BeginCameraAngleAdjust();

	/** Taste losgelassen: startet die Rueckfahrt zum gemerkten Winkel. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void EndCameraAngleAdjust();

	/** Mausbewegung waehrend des Haltens. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddCameraAngleInput(float PitchInput, float YawInput);

private:
	/** Der beim Druecken gemerkte Winkel, Ziel der Rueckfahrt. */
	FRotator SavedCameraAngle = FRotator::ZeroRotator;

	/** Das mit der Maus wandernde Ziel. Der sichtbare Winkel laeuft ihm geglaettet hinterher. */
	FRotator TargetCameraAngle = FRotator::ZeroRotator;

	/** Gefuehrte Scrollgeschwindigkeit fuer PanMoveCameraSmoothed. */
	FVector CurrentPanVelocity = FVector::ZeroVector;

	/** Fuehrt die Rueckfahrt aus; wird aus Tick gerufen. */
	void TickCameraAngleReturn(float DeltaTime);

	/** Fuehrt den sichtbaren Winkel an das Mausziel heran; wird aus Tick gerufen. */
	void TickCameraAngleSmoothing(float DeltaTime);

public:

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

	/**
	 * Wie PanMoveCamera, aber mit Anlauf und Auslauf.
	 *
	 * Das ruckartige Gefuehl am Bildschirmrand kam vom harten Ein/Aus: sobald die Maus die Kante
	 * beruehrte, lief die Kamera mit voller Geschwindigkeit los, und beim Verlassen stand sie
	 * schlagartig. Hier wird stattdessen eine Geschwindigkeit gefuehrt, die dem Ziel folgt.
	 */
	UFUNCTION( BlueprintCallable, Category = RTSUnitTemplate)
		void PanMoveCameraSmoothed(const FVector& NewPanDirection, float DeltaTime);

	/** Setzt die gefuehrte Scrollgeschwindigkeit sofort auf null. */
	UFUNCTION( BlueprintCallable, Category = RTSUnitTemplate)
		void ResetPanVelocity();

	/**
	 * Anlauf am Bildschirmrand (1/s). Groesser = schneller auf Tempo.
	 * Bei 0 verhaelt sich das Randscrollen wieder wie frueher, also ohne Anlauf.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float EdgeScrollAcceleration = 9.f;

	/**
	 * Auslauf, wenn die Maus die Kante verlaesst (1/s). Absichtlich hoeher als der Anlauf -
	 * ein langes Nachgleiten fuehlt sich wie ein Steuerungsfehler an, nicht wie Weichheit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float EdgeScrollDeceleration = 14.f;

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
		float AccelerationRate = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float DecelerationRate = 25000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float CamSpeed = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ZoomSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float FastZoomSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float AutoZoomSpeed = 25.f;

	/**
	 * Faktor, mit dem die pro-Frame-Schritte der Kamera auf 60 FPS normiert werden.
	 *
	 * Zoom, Rotation und Bewegungsbeschleunigung wurden urspruenglich pro Frame addiert,
	 * nicht pro Sekunde - damit lief die Kamera auf einer schnellen Maschine um ein
	 * Vielfaches schneller. Bei 60 FPS liefert das hier 1.0, die vorhandenen Werte behalten
	 * also genau ihre eingestellte Wirkung; bei 200 FPS 0.3, bei 30 FPS 2.0.
	 * Nach oben begrenzt, damit ein einzelner Hitch die Kamera nicht wegschleudert.
	 */
	float GetFrameScale() const;

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
		void RotateSpringArmPitchFree(bool Invert);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void RotateSpringArmYawFree(bool Invert);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool RotateFree(FVector MouseLocation);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool RotateCamera(float Direction, float Add, bool stopCam = false);

	/**
	 * Dreht den Federarm sanft auf einen Ziel-Yaw und nimmt dabei immer den kuerzeren Weg.
	 *
	 * Bewusst NICHT ueber RotateCamera: das rampt ueber CurrentRotationValue auf eine feste
	 * Schrittweite hoch und dreht dann gleichfoermig weiter - gut fuer gehaltene Tasten, aber
	 * es schiesst ueber ein Ziel hinaus und pendelt. Hier wird stattdessen ein Anteil der
	 * Restdifferenz pro Sekunde abgebaut: schnell bei grosser Abweichung, sanft auslaufend
	 * kurz vor dem Ziel.
	 *
	 * @param TargetYaw    Ziel-Yaw in Weltgrad.
	 * @param InterpSpeed  Wie zuegig nachgezogen wird (1/s). 0 = keine Bewegung.
	 * @param DeltaTime    Vergangene Zeit; die Drehung ist damit bildratenunabhaengig.
	 * @param Toleranz     Ab welcher Restdifferenz das Ziel als erreicht gilt (Grad).
	 * @return true, sobald die Restdifferenz unter der Toleranz liegt.
	 */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool RotateCamYawTowards(float TargetYaw, float InterpSpeed, float DeltaTime, float Toleranz = 0.5f);
	
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

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void LockOnActor(AActor* Actor);
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float AddCamRotationSpeaking = 0.75f;
	
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
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveInDirection(FVector Direction, float DeltaTime);
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool SwapScroll = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UUserWidget* ControlWidget;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "HideControlWidget", Keywords = "RTSUnitTemplate HideControlWidget"), Category = RTSUnitTemplate)
		void HideControlWidget();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ShowControlWidget", Keywords = "RTSUnitTemplate ShowControlWidget"), Category = RTSUnitTemplate)
		void ShowControlWidget();

	//UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		//void SetControlWidgetLocation();
	
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
