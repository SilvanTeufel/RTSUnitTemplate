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

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_TravelToMap(const FString& MapName, FName TagToEnable = NAME_None);


	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpdateCameraUnitMovement(const FVector& TargetLocation);

	UPROPERTY(EditAnywhere, Category = "RTS|Network")
	float CameraUnitUpdateInterval = 0.15f; // 150ms für Einheiten-Bewegung

	UPROPERTY(EditAnywhere, Category = "RTS|Network")
	float CameraSyncInterval = 0.1f; // 100ms für Kamera-Sync

	// Interne Timer
	float CameraUnitUpdateTimer = 0.0f;
	float CameraSyncTimer = 0.0f;

	// Cache für die letzte Position zur Vermeidung redundanter Pakete
	FVector LastSyncedCameraLocation = FVector::ZeroVector;
	
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

	/**
	 * Shown the INSTANT a level travel is kicked off. ServerTravel only sets World->NextURL; the engine
	 * finishes the frame and loads the new map on a later TickWorldTravel, so without this the player
	 * keeps staring at the old level for the whole load. Covers that gap; the map change tears the
	 * widget down on its own. This is separate from Client_ShowLoadingWidget, which runs AFTER arrival.
	 */
	UFUNCTION(Client, Reliable)
	void Client_ShowTravelLoadingScreen();

	/** Widget used by Client_ShowTravelLoadingScreen. Falls back to the GameState's
	 *  LoadingWidgetConfig.WidgetClass when left unset, so existing levels need no extra setup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<class ULoadingWidget> TravelLoadingWidgetClass = nullptr;

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

	/**
	 * Geglaettete Fassung von UnitCountInRange, nur fuer das Zoomziel der AutoCam.
	 * Die rohe Zahl springt in einer Schlacht jeden Frame; direkt verrechnet laesst sie das
	 * Zoomziel um UnitZoomScaler * Schwankung zittern.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate")
	float SmoothedUnitCountInRange = 0.f;

	/** Wie schnell SmoothedUnitCountInRange nachzieht. Klein = ruhiger, traeger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float UnitCountSmoothingSpeed = 1.5f;
	
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
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LockCamToSpecificUnit", Keywords = "TopDownRTSCamLib LockCamToSpecificUnit"), Category = RTSUnitTemplate)
	void LockCamToSpecificUnit(AUnitBase* SUnit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ASpeakingUnit* SpeakingUnit;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LockCamToCharacter(int Index);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LockCamToCharacterWithTag(float DeltaTime);
	
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
	bool RotateBehindCharacterIfLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CameraUnitMouseFollow = true;

	// ============================================================================================
	// Kamera sanft hinter die CameraUnit schwenken (22.08.2026)
	//
	// Gedacht fuer den Fall CameraUnitMouseFollow == false, in dem die Einheit direkt gesteuert
	// wird: die Kamera zieht dann von selbst hinter sie, sobald der Spieler sie eine Weile nicht
	// mehr gedreht hat. Dreht er selbst (Q/E), setzt das Nachfuehren aus und beginnt erst nach
	// RotateCamBehindDelayAfterManual wieder - sonst kaempfte die Automatik gegen die Eingabe.
	//
	// Standard ist AUS, damit sich das Verhalten bestehender Projekte nicht aendert.
	// AstraHelix schaltet es in seinen Controller-Blueprints ein.
	//
	// Unterschied zu RotateBehindCharacterIfLocked: jenes dreht in festen Schritten
	// (AddCamRotation * 2) mit 10 Grad Totzone und ist an LockCameraToCharacter gebunden.
	// Hier wird ueber RotateCamYawTowards ein Anteil der Restdifferenz abgebaut, also sanft
	// auslaufend statt gleichfoermig.
	// ============================================================================================

	/** Kamera von selbst hinter die CameraUnit drehen, solange der Spieler nicht selbst dreht. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Kamera hinter Einheit")
	bool bRotateCamBehindCharacter = false;

	/** Wie zuegig nachgezogen wird (1/s). Groesser = strafferes Nachziehen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Kamera hinter Einheit", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "10.0"))
	float RotateCamBehindSpeed = 3.0f;

	/** Wartezeit nach einer eigenen Drehung, bevor die Kamera wieder von selbst nachzieht (Sekunden). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Kamera hinter Einheit", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float RotateCamBehindDelayAfterManual = 1.5f;

	/** Totbereich in Grad - darunter wird nicht nachgeregelt, damit die Kamera nicht zittert. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Kamera hinter Einheit", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float RotateCamBehindDeadzone = 2.0f;

	/**
	 * Aufschlag auf die Blickrichtung der Einheit, in Grad.
	 *
	 * Welcher Federarm-Yaw "hinter der Einheit" bedeutet, haengt davon ab, wie der SpringArm
	 * im jeweiligen Kamera-Blueprint aufgebaut ist. Steht die Kamera nach dem Einschalten
	 * VOR der Einheit statt dahinter, ist 180 der richtige Wert.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Kamera hinter Einheit", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float RotateCamBehindYawOffset = 0.0f;

	/** Restliche Wartezeit nach einer eigenen Drehung. Nicht editierbar, laeuft zur Laufzeit. */
	float RotateCamBehindCooldown = 0.f;

	// ============================================================================================
	// LUX-ANPASSUNG 1/3 â€” WASD steuert die CameraUnit (16.08.2026)
	// Muss beim Uebernehmen ins Original-Template mitwandern. Siehe REAPPLY_AFTER_PLUGIN_SWAP.md.
	// ============================================================================================
	// Greift, wenn eine CameraUnit gesetzt ist UND CameraUnitMouseFollow == false.
	// Dann steuert WASD die EINHEIT (statt die Kamera zu schwenken) und die Kamera bleibt
	// ueber ihr stehen. Die Bewegung laeuft ueber dasselbe MoveTarget/Client-Prediction-
	// Fragment wie das Maus-Folgen, damit Navigation und Replikation unveraendert greifen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lux Direktsteuerung")
	bool bUnitDirectControl = true;

	// Wie weit vor der Einheit das Laufziel liegt.
	// NICHT zu klein waehlen: erreicht die Einheit ihr Ziel, haelt sie wegen
	// IntentAtGoal = Stand an und laeuft erst beim naechsten Update weiter - das ruckelt.
	// Der Wert muss > MovementAcceptanceRadius (50) und > Strecke pro Update sein
	// (BaseRunSpeed * UnitDirectMoveUpdateInterval, also ~700*0.03 = 21 uu).
	// Kurzes Antippen bleibt trotzdem kurz, weil beim Loslassen hart gestoppt wird.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lux Direktsteuerung")
	float UnitDirectMoveLookAhead = 500.f;

	// Eigenes, kuerzeres Intervall als CameraUnitUpdateInterval: beim Maus-Folgen wandert das
	// Ziel langsam, bei WASD muss es jede Richtungsaenderung zeitnah nachfuehren.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lux Direktsteuerung")
	float UnitDirectMoveUpdateInterval = 0.03f;

	// Wie schnell die Kamera der Einheit nachzieht (VInterpTo-Speed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lux Direktsteuerung")
	float UnitDirectCamFollowSpeed = 8.f;

	// Auslaufstrecke beim Loslassen der Taste, in uu. Die Einheit bekommt ein Ziel so weit
	// voraus und wird von der Mass-Ankunftslogik dorthin ausgebremst - das gibt ihr etwas
	// Traegheit, statt schlagartig zu stehen. 0 = harter Stopp wie zuvor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Lux Direktsteuerung")
	float UnitDirectStopGlide = 90.f;

	// Haelt die Einheit HART an (Stand + Speed 0) statt sie zum aktuellen Standort
	// "laufen" zu lassen. Ohne das rollte sie nach dem Loslassen weiter aus, weil
	// Server_UpdateCameraUnitMovement intern UpdateMoveTarget mit voller BaseRunSpeed
	// aufruft und der StopMovement-Tag nur verzoegert (Defer) greift.
	UFUNCTION(Server, Reliable)
	void Server_StopCameraUnitDirect();

	float UnitDirectMoveTimer = 0.f;
	float UnitDirectHoldTime = 0.f;
	bool bUnitDirectWasMoving = false;

	// ============================================================================================
	// LUX-ANPASSUNG 5/5 - Client-Vorhersage fuer die Direktsteuerung (16.08.2026)
	// Silvan: "Die Bewegung der CameraUnit (W,A,S,D) bei MouseFollow = false, ist auf dem
	// Clienten EXTREM laggy ... Auf dem Server ist es gut."
	// Ursache: der WASD-Pfad schickte NUR Server_UpdateCameraUnitMovement. Auf dem Client
	// bewegte sich die Einheit damit erst, nachdem der Server den MoveTarget gesetzt UND die
	// neue Position zurueckrepliziert hatte - also volle Roundtrip- plus Replikationszeit pro
	// Richtungsaenderung. Der normale Rechtsklick-Befehl hat dieses Problem nicht, weil er in
	// ACustomControllerBase::ApplyMovePredictionToUnit zusaetzlich lokal vorhersagt
	// (FMassClientPredictionFragment). Genau das holt diese Funktion fuer WASD nach.
	// ============================================================================================
	// bStartingMove: nur im ERSTEN Takt einer Bewegung true. Das Setzen von Laufzustand und
	// Mass-Tag gehoert dorthin - jeden Frame ausgefuehrt macht es die Steuerung traege.
	void ApplyDirectMovePredictionLocally(const FVector& Target, bool bStopping, bool bStartingMove = false);

	// Letzte WASD-Laufrichtung in Weltkoordinaten - fuer das lokale Auslaufen beim Loslassen.
	FVector LastUnitDirectWorldDir = FVector::ZeroVector;

	// Fuer das gemessene Tempo der Client-Animation (siehe ApplyDirectMovePredictionLocally).
	FVector LetzterVorhersageOrt = FVector::ZeroVector;
	float LetzteVorhersageZeit = 0.f;

	// Nur fuer die Startdiagnose (siehe [StartDiag] in der .cpp).
	float DiagnoseStartZeit = 0.f;
	FVector DiagnoseStartOrt = FVector::ZeroVector;
	// ===================== ENDE LUX-ANPASSUNG 1/3 ===============================================

	// ============================================================================================
	// LUX-ANPASSUNG (28.08.2026) - Klick beim Zielen gehoert der zielenden Faehigkeit.
	// Muss beim Uebernehmen ins Original-Template mitwandern. Siehe REAPPLY_AFTER_PLUGIN_SWAP.md.
	//
	// Steht der Ziel-Indikator einer Faehigkeit mit bIndicatorClicksAdvanceAbility, leitet der
	// Linksklick der Direktsteuerung an FireAbilityMouseHit weiter (ClickCount++), statt AbilityOne
	// neu zu starten. Rueckgabe true = der Klick ist verbraucht, der Aufrufer darf nichts weiter tun.
	// ============================================================================================
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Lux Direktsteuerung")
	bool LuxTryAdvanceIndicatorAbilityWithClick();
	// ===================== ENDE LUX-ANPASSUNG ===================================================

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
	// Helper functions for scroll zoom logic
	void HandleScrollZoomIn();
	void HandleScrollZoomOut();

};
