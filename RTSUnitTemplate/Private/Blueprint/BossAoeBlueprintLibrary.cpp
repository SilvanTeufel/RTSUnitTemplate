// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Blueprint/BossAoeBlueprintLibrary.h"

#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UBossAoeBlueprintLibrary::GetEnemyPlayerUnits(AUnitBase* Boss, TArray<AUnitBase*>& OutUnits)
{
	OutUnits.Reset();

	UWorld* Welt = Boss ? Boss->GetWorld() : nullptr;
	if (!Welt)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = Welt->GetPlayerControllerIterator(); It; ++It)
	{
		AControllerBase* PC = Cast<AControllerBase>(It->Get());
		if (!PC)
		{
			continue;
		}

		AUnitBase* Einheit = PC->CameraUnitWithTag;
		if (!IsValid(Einheit))
		{
			continue;
		}

		// Nur Gegner des Bosses. Ohne diese Pruefung zielt ein Boss auch auf verbuendete
		// Kameraeinheiten, sobald mehrere Teams eine haben.
		if (Einheit->TeamId == Boss->TeamId)
		{
			continue;
		}

		OutUnits.AddUnique(Einheit);
	}
}

namespace
{
	/** Anker plus die Angabe, ob er auf einem Spieler liegt. */
	struct FAnker
	{
		FVector Ort = FVector::ZeroVector;
		bool bAufSpieler = false;
	};

	FAnker AnkerBestimmen(AUnitBase* Boss, float PlayerChance, bool bFallBackToChasedUnit)
	{
		FAnker Ergebnis;
		Ergebnis.Ort = Boss->GetActorLocation();

		if (PlayerChance <= 0.f || FMath::FRand() > PlayerChance)
		{
			return Ergebnis;
		}

		TArray<AUnitBase*> Spieler;
		UBossAoeBlueprintLibrary::GetEnemyPlayerUnits(Boss, Spieler);
		if (Spieler.Num() > 0)
		{
			Ergebnis.Ort = Spieler[FMath::RandRange(0, Spieler.Num() - 1)]->GetActorLocation();
			Ergebnis.bAufSpieler = true;
			return Ergebnis;
		}

		// Keine gesteuerte Figur da (reines RTS-Team, oder Kameraeinheit noch nicht gesetzt):
		// dann wenigstens auf das aktuelle Ziel des Bosses zielen statt auf ihn selbst.
		if (bFallBackToChasedUnit && IsValid(Boss->UnitToChase))
		{
			Ergebnis.Ort = Boss->UnitToChase->GetActorLocation();
			Ergebnis.bAufSpieler = true;
		}
		return Ergebnis;
	}
}

FVector UBossAoeBlueprintLibrary::GetAoeSpawnLocation(AUnitBase* Boss, float Range,
                                                      float PlayerChance, float MinRadius,
                                                      float PlayerSpread)
{
	if (!IsValid(Boss))
	{
		return FVector::ZeroVector;
	}

	const FAnker Anker = AnkerBestimmen(Boss, PlayerChance, true);

	const float Radius = Anker.bAufSpieler
		? FMath::FRandRange(0.f, FMath::Max(0.f, PlayerSpread))
		: FMath::FRandRange(MinRadius, FMath::Max(MinRadius, Range));

	const FRotator Richtung(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
	return Anker.Ort + Richtung.Vector() * Radius;
}

FVector UBossAoeBlueprintLibrary::GetAoeAnchorLocation(AUnitBase* Boss, float PlayerChance,
                                                       bool bFallBackToChasedUnit)
{
	if (!IsValid(Boss))
	{
		return FVector::ZeroVector;
	}

	return AnkerBestimmen(Boss, PlayerChance, bFallBackToChasedUnit).Ort;
}
