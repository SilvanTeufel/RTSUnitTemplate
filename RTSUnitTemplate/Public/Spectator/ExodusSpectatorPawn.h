// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "ExodusSpectatorPawn.generated.h"

/**
 * Read-only spectator camera. Keeps the full camera-navigation binding of AExtendedCameraBase but
 * overrides BindGameplayInputActions to an empty body (Strategy A: the SetupPlayerInputComponent
 * split), so no select/command/ability/control-group input is ever bound. Re-applies client-local camera
 * state (IMC, controller CameraBase pointer, BlockControls, spring-arm height) after an ack-gated
 * ClientRestart, and signals readiness back to the server so AExodusGameMode can drive vision + fog/minimap.
 */
UCLASS()
class RTSUNITTEMPLATE_API AExodusSpectatorPawn : public AExtendedCameraBase
{
	GENERATED_BODY()

public:
	AExodusSpectatorPawn(const FObjectInitializer& ObjectInitializer);

	/** Client-side possession hooks. */
	virtual void BeginPlay() override;
	virtual void OnRep_Controller() override;   // fires on owning client when Controller replicates
	virtual void PawnClientRestart() override;  // fired by the server's gated ClientRestart

	/** Server-side handshake: pawn tells the server it exists & is locally controlled on the client. */
	UFUNCTION(Server, Reliable)
	void Server_ConfirmSpectatorReady();

	// ---- Server-only bookkeeping (NOT replicated) ----
	/** Guards Server_ConfirmSpectatorReady / FinishSpectateSetup against re-entrancy. */
	UPROPERTY(Transient)
	bool bSpectatorReadyHandled = false;

	/** Reveal-all requested at spawn time; consumed by the server ack handler. */
	UPROPERTY(Transient)
	bool bPendingRevealAll = false;

	/** The player's real team id, captured before we (optionally) switch vision to team 0. */
	UPROPERTY(Transient)
	int32 OriginalTeamId = -1;

	// ---- Tunables ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exodus|Spectator")
	float SpectatorArmLength = 2500.f;   // spring-arm height on entering spectate

protected:
	/** Camera-only pawn: deliberately bind NOTHING here (no select/command/ability/control-group).
	 *  The inherited SetupPlayerInputComponent still calls BindCameraInputActions for full pan/zoom/rotate. */
	virtual void BindGameplayInputActions(class UEnhancedInputComponentBase* EIC) override;

	/** Client-local: IMC + controller CameraBase + BlockControls=false + spring-arm height. */
	void ApplySpectatorClientState();

	/** Client-local: send the readiness ack exactly once, once locally controlled. */
	void TrySendSpectatorAck();

	// Client-only guard so the ack is sent once per pawn.
	bool bAckSent = false;
};
