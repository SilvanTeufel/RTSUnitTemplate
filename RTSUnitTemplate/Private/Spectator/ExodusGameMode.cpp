// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Spectator/ExodusGameMode.h"
#include "Spectator/ExodusSpectatorPawn.h"

#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Camera/CameraBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"            // UWorld::LineTraceSingleByChannel (explicit — adaptive-unity can hide this)
#include "CollisionQueryParams.h"   // FCollisionQueryParams / ECC_Visibility

void AExodusGameMode::EnterSpectate(AController* Controller, bool bRevealAll)
{
	if (!HasAuthority() || !Controller) return;

	ACustomControllerBase* PC = Cast<ACustomControllerBase>(Controller);
	if (!PC) return;

	// ---- IDEMPOTENT UPGRADE PATH ----
	// Already spectating? The pawn's class IS the state. Just (re)drive the finish step
	// (e.g. own-team -> reveal-all). No respawn, no new ack needed (pawn already exists/possessed).
	if (AExodusSpectatorPawn* Existing = Cast<AExodusSpectatorPawn>(PC->GetPawn()))
	{
		FinishSpectateSetup(PC, Existing, bRevealAll);
		return;
	}

	// ---- FIRST-TIME SPECTATE ----
	APawn* OldPawn = PC->GetPawn();
	const FVector OldLoc = OldPawn ? OldPawn->GetActorLocation() : PC->GetSpawnLocation();
	const int32   RealTeamId = PC->SelectableTeamId;                 // ControllerBase.h:327
	const FVector SpawnLoc   = ResolveGroundSpawn(OldLoc, OldPawn);

	// Detach the old camera pawn first so Possess() below is clean.
	if (OldPawn) PC->UnPossess();

	// Deferred spawn so we can seed server-only fields before BeginPlay.
	// (Assign via if instead of a ?: — a TSubclassOf/UClass* ternary is an ambiguous-conversion error, C2445.)
	TSubclassOf<AExodusSpectatorPawn> Cls = SpectatorPawnClass;
	if (!Cls)
	{
		Cls = AExodusSpectatorPawn::StaticClass();
	}
	const FTransform Xf(FRotator::ZeroRotator, SpawnLoc);

	AExodusSpectatorPawn* Spectator = GetWorld()->SpawnActorDeferred<AExodusSpectatorPawn>(
		Cls, Xf, PC, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Spectator)
	{
		if (OldPawn) PC->Possess(OldPawn);   // roll back
		return;
	}
	Spectator->OriginalTeamId    = RealTeamId;
	Spectator->bPendingRevealAll = bRevealAll;
	UGameplayStatics::FinishSpawningActor(Spectator, Xf);

	PC->Possess(Spectator);                  // sets Spectator->Owner = PC (enables its Server RPC)

	if (OldPawn) OldPawn->Destroy();         // safe: already unpossessed

	// Spectator flag AFTER possess. Use SetIsSpectator (NOT bOnlySpectator): APlayerController::OnPossess
	// early-outs on IsOnlyASpectator(), which reads bOnlySpectator — setting that would break possession.
	if (APlayerState* PS = PC->PlayerState)
	{
		PS->SetIsSpectator(true);
	}

	if (PC->IsLocalController())
	{
		// Listen-server host / standalone: pawn is local, ClientRestart runs inline. No round-trip.
		Spectator->bSpectatorReadyHandled = true;
		FinishSpectateSetup(PC, Spectator, bRevealAll);
	}
	else
	{
		// Remote client: wait for AExodusSpectatorPawn::Server_ConfirmSpectatorReady (driven by
		// OnRep_Controller). Belt-and-suspenders: fall back after a timeout if the ack never lands.
		TWeakObjectPtr<AExodusGameMode> WeakThis(this);
		TWeakObjectPtr<ACustomControllerBase> WeakPC(PC);
		TWeakObjectPtr<AExodusSpectatorPawn> WeakSpec(Spectator);
		FTimerHandle Tmp;
		GetWorldTimerManager().SetTimer(Tmp, [WeakThis, WeakPC, WeakSpec, bRevealAll]()
		{
			AExodusGameMode* GM = WeakThis.Get();
			AExodusSpectatorPawn* Spec = WeakSpec.Get();
			ACustomControllerBase* P = WeakPC.Get();
			if (GM && Spec && P && !Spec->bSpectatorReadyHandled)
			{
				Spec->bSpectatorReadyHandled = true;
				GM->FinishSpectateSetup(P, Spec, bRevealAll);
			}
		}, SpectatorAckTimeout, false);
	}
}

void AExodusGameMode::FinishSpectateSetup(ACustomControllerBase* PC, AExodusSpectatorPawn* Spectator, bool bRevealAll)
{
	if (!HasAuthority() || !PC || !Spectator) return;
	// Guard: the pawn must still be the controller's current pawn (ignore stale acks/timers).
	if (PC->GetPawn() != Spectator) return;

	// (1) Server-side CameraBase re-point (server logic + replicates CameraBase to PC's client).
	PC->InitCameraHUDGameMode();                         // ControllerBase.h:80

	// (2) Vision. Reveal-all => team 0 (flips UnitVisibilityProcessor's bSpectator AND, via
	//     OnTeamIdChanged, disables the per-team fog PP because no fog actor has TeamId 0).
	//     Own-team => restore the captured real id (also downgrades a prior reveal-all correctly).
	if (bRevealAll)
	{
		PC->Multi_SetControllerTeamId(0);                // ControllerBase.h:342
	}
	else
	{
		PC->Multi_SetControllerTeamId(Spectator->OriginalTeamId);   // verify-fix: else-branch, was silently missing
	}

	// (3) Force the client's input rebuild + our PawnClientRestart override (IMC / CameraBase /
	//     BlockControls / spring-arm). Reliable & ordered before the multicasts below.
	PC->ClientRestart(Spectator);

	// (4) Server-authoritative unblock (replicates BlockControls -> client).
	if (PC->CameraBase)
	{
		PC->CameraBase->BlockControls = false;           // CameraBase.h:64
	}

	// (5) Rebuild the MainHUD bound to the NEW pawn so ExtendedCameraBase->Minimap is (re)assigned,
	//     then re-init fog + minimap for the (possibly new) vision id.
	PC->Client_InitializeMainHUD();                      // CustomControllerBase.h:302
	PC->Multi_InitFogOfWar();                            // CustomControllerBase.h:85
	PC->Multi_SetupPlayerMiniMap();                      // CustomControllerBase.h:270
}

FVector AExodusGameMode::ResolveGroundSpawn(const FVector& XYFrom, const AActor* IgnoreActor) const
{
	const FVector Start(XYFrom.X, XYFrom.Y, XYFrom.Z + 100.f);
	const FVector End  (XYFrom.X, XYFrom.Y, XYFrom.Z - 100000.f);

	FHitResult Hit;
	FCollisionQueryParams Q(FName(TEXT("SpectatorGroundZ")), /*bTraceComplex=*/false);
	if (IgnoreActor) Q.AddIgnoredActor(IgnoreActor);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q))   // idiom: CustomControllerBase.cpp
	{
		return FVector(XYFrom.X, XYFrom.Y, Hit.Location.Z + SpectatorGroundZOffset);
	}
	return FVector(XYFrom.X, XYFrom.Y, SpectatorFallbackZ);
}
