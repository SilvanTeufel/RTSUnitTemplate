// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Spectator/ExodusSpectatorPawn.h"
#include "Spectator/ExodusGameMode.h"

#include "Controller/PlayerController/CustomControllerBase.h"
#include "Controller/Input/EnhancedInputComponentBase.h"   // UEnhancedInputComponentBase (override param type)
#include "Controller/Input/GameplayTags.h"                 // FGameplayTags (parity with base input TU)
#include "Core/UnitData.h"                                 // CameraData::UseScreenEdges
#include "EnhancedInputSubsystems.h"                        // UEnhancedInputLocalPlayerSubsystem
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"                                   // GetWorld()->GetAuthGameMode<T>() (explicit — adaptive-unity can hide this)

AExodusSpectatorPawn::AExodusSpectatorPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A spectator never blocks map progress and should replicate to its owning client.
	bReplicates = true;
	// BlockControls (CameraBase.h:64) defaults true; we clear it on the client in ApplySpectatorClientState.
}

void AExodusSpectatorPawn::BindGameplayInputActions(UEnhancedInputComponentBase* /*EIC*/)
{
	// Intentionally empty: read-only spectator. The inherited SetupPlayerInputComponent still calls
	// BindCameraInputActions (full pan/zoom/rotate/orbit); we only suppress the gameplay bindings.
}

void AExodusSpectatorPawn::BeginPlay()
{
	// Runs on server AND client. On the client this may fire before possession resolves, so the
	// IMC add inside ACameraBase::BeginPlay can no-op — that's expected and is repaired later in
	// PawnClientRestart/ApplySpectatorClientState.
	Super::BeginPlay();
	TrySendSpectatorAck();   // in case Controller was already valid on the client
}

void AExodusSpectatorPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	// Owning-client signal that the possession handshake resolved. Ack the server.
	TrySendSpectatorAck();
}

void AExodusSpectatorPawn::PawnClientRestart()
{
	// Called by the server's gated ClientRestart. Super rebuilds the input component using the
	// inherited SetupPlayerInputComponent (camera-only, since our BindGameplayInputActions is empty).
	Super::PawnClientRestart();
	ApplySpectatorClientState();
	TrySendSpectatorAck();
}

void AExodusSpectatorPawn::TrySendSpectatorAck()
{
	if (bAckSent) return;
	AController* C = GetController();
	if (C && C->IsLocalPlayerController())
	{
		bAckSent = true;
		Server_ConfirmSpectatorReady();
	}
}

void AExodusSpectatorPawn::ApplySpectatorClientState()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController()) return;

	// (1) (Re)add the camera IMC — ACameraBase::BeginPlay's add may have been skipped pre-possession.
	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsys =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			if (MappingContext)   // CameraBase.h:91
			{
				Subsys->RemoveMappingContext(MappingContext);
				Subsys->AddMappingContext(MappingContext, MappingPriority);
			}
		}
	}

	// (2) Re-point the controller's CameraBase at THIS pawn (was pointing at the destroyed pawn).
	//     InitCameraHUDGameMode() does CameraBase = Cast<ACameraBase>(GetPawn()) (+ HUDBase/RTSGameMode).
	if (ACustomControllerBase* CPC = Cast<ACustomControllerBase>(PC))
	{
		CPC->InitCameraHUDGameMode();   // ControllerBase.h:80
	}

	// (3) Unblock camera navigation locally (server also clears it; replicated).
	BlockControls = false;              // CameraBase.h:64

	// (4) Spring-arm height / zoom.
	if (SpringArm)                      // CameraBase.h:82
	{
		SpringArm->TargetArmLength = SpectatorArmLength;
	}
	// NOTE: ZoomPosition (CameraBase.h:220) defaults to 75.f and is NOT plainly a spring-arm length; the
	// zoom state machine reconciles it against TargetArmLength. We seed it to SpectatorArmLength so the
	// first scroll doesn't fight the value we just set on the spring arm. If a snap is observed on the
	// first scroll, tune SpectatorArmLength / ZoomPosition together.
	ZoomPosition = SpectatorArmLength;
	SetCameraState(CameraData::UseScreenEdges);   // CameraBase.h:291
}

void AExodusSpectatorPawn::Server_ConfirmSpectatorReady_Implementation()
{
	if (bSpectatorReadyHandled) return;   // idempotent: ignore duplicate acks
	bSpectatorReadyHandled = true;

	ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetController());
	AExodusGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AExodusGameMode>() : nullptr;
	if (PC && GM)
	{
		GM->FinishSpectateSetup(PC, this, bPendingRevealAll);
	}
}
