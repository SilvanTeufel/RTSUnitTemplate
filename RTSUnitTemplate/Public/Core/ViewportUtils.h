// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace RTSViewportUtils
{
	/**
	 * True when WorldLocation projects into the LOCAL viewport rect, inflated by Offset pixels.
	 *
	 * This is the one-shot counterpart of FMassVisibilityFragment::bIsOnViewport: that flag is
	 * recomputed at 20 Hz by UUnitVisibilityProcessor and is only meaningful for an entity that owns
	 * it, while effects have to be culled at the exact moment they fire, at a point that may belong
	 * to no entity at all (a muzzle offset, an impact on terrain). Same rect maths as the processor,
	 * same default 150 px margin (APerformanceUnit::VisibilityOffset / AProjectile::VisibilityOffset),
	 * so both paths agree on what "on screen" means.
	 *
	 * Evaluated per viewer, never replicated: every machine answers for its own camera.
	 *
	 * Returns false when there is no local viewer at all (dedicated server) - callers use this as a
	 * "may this local machine show it" test, and a dedicated server must show nothing.
	 */
	inline bool IsLocationOnLocalViewport(const UObject* WorldContextObject, const FVector& WorldLocation, float Offset)
	{
		if (!WorldContextObject)
		{
			return false;
		}

		UWorld* World = WorldContextObject->GetWorld();
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return false;
		}

		// ProjectWorldToScreen reads the local player's cached view state, which is game-thread only
		// (UUnitVisibilityProcessor sets bRequiresGameThreadExecution for exactly this reason).
		// Off-thread we must not guess: report visible so a caller can at worst play a redundant
		// effect instead of silently losing every effect.
		if (!IsInGameThread())
		{
			return true;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController())
			{
				continue;
			}

			FVector2D ScreenPosition;
			// The return value is load-bearing: for a point behind the camera ProjectWorldToScreen
			// fails and leaves ScreenPosition untouched, so ignoring it reads as (0,0) - i.e. dead
			// centre of the screen - and everything behind you counts as visible.
			if (!UGameplayStatics::ProjectWorldToScreen(PC, WorldLocation, ScreenPosition))
			{
				continue;
			}

			int32 SizeX = 0;
			int32 SizeY = 0;
			PC->GetViewportSize(SizeX, SizeY);
			if (SizeX <= 0 || SizeY <= 0)
			{
				continue;
			}

			if (ScreenPosition.X >= -Offset && ScreenPosition.X <= (float)SizeX + Offset &&
				ScreenPosition.Y >= -Offset && ScreenPosition.Y <= (float)SizeY + Offset)
			{
				// Split screen: one local viewer seeing it is enough, the effect is shared.
				return true;
			}
		}

		return false;
	}
}
