// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/UpgradeGameMode.h"   // same base as BP_GameModeBase_0
#include "ExodusGameMode.generated.h"

class ACustomControllerBase;
class AExodusSpectatorPawn;

/**
 * GameMode adding a read-only spectate flow. Overrides ARTSGameModeBase::EnterSpectate to
 * swap a defeated player's camera pawn for an AExodusSpectatorPawn and drive the client re-init.
 */
UCLASS()
class RTSUNITTEMPLATE_API AExodusGameMode : public AUpgradeGameMode
{
	GENERATED_BODY()

public:
	/** Spectator pawn class to spawn. Assign a BP subclass of AExodusSpectatorPawn that carries the
	 *  MainHUD/Minimap widget wiring; falls back to the C++ class (no minimap) if unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exodus|Spectator")
	TSubclassOf<AExodusSpectatorPawn> SpectatorPawnClass;

	/** Added to the traced ground Z when placing the spectator pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exodus|Spectator")
	float SpectatorGroundZOffset = 250.f;

	/** Z used when the ground trace misses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exodus|Spectator")
	float SpectatorFallbackZ = 250.f;

	/** Server fallback delay if the client readiness ack is lost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exodus|Spectator")
	float SpectatorAckTimeout = 0.75f;

	/**
	 * Server-side, IDEMPOTENT. First call swaps the controller's pawn for an AExodusSpectatorPawn
	 * (read-only camera) at the old pawn's X/Y and ground Z, marks the PlayerState a spectator, and
	 * ack-gates the client input setup. A subsequent call upgrades vision in place without re-spawning.
	 *
	 * @param Controller  the player's controller (expects an ACustomControllerBase-derived PC).
	 * @param bRevealAll  true => vision team 0 (reveal everything, no fog); false => keep real team id.
	 */
	virtual void EnterSpectate(AController* Controller, bool bRevealAll) override;

	/** Idempotent finish step: vision id + forced ClientRestart + HUD/fog/minimap re-init.
	 *  Public because AExodusSpectatorPawn::Server_ConfirmSpectatorReady calls it. */
	void FinishSpectateSetup(ACustomControllerBase* PC, AExodusSpectatorPawn* Spectator, bool bRevealAll);

protected:
	/** Downward line trace to place the spectator on the ground under (XYFrom.X, XYFrom.Y). */
	FVector ResolveGroundSpawn(const FVector& XYFrom, const AActor* IgnoreActor) const;
};
