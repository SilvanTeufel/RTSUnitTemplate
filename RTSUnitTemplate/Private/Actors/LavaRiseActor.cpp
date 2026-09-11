// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Actors/LavaRiseActor.h"

#include "Characters/Unit/UnitBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ALavaRiseActor::ALavaRiseActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Der Anstieg laeuft nur auf dem Server (Tick prueft HasAuthority). Ohne Bewegungsreplikation
	// bekommt der Client die neue Hoehe nie - die Lava stand dort still, waehrend sie auf dem
	// Server stieg, und der Schaden kam scheinbar aus dem Nichts.
	SetReplicateMovement(true);
}

void ALavaRiseActor::BeginPlay()
{
	Super::BeginPlay();

	if (!LavaActor)
	{
		// Kein Ziel gesetzt: sich selbst anheben. Damit laesst sich die Lava als EIN Blueprint
		// bauen (Aufstiegslogik + Flaeche als Komponente), statt zwei Aktoren im Level zu
		// verdrahten - und die Werte liegen im Blueprint statt als Instanz-Ueberschreibung.
		LavaActor = this;
		UE_LOG(LogTemp, Log,
		       TEXT("[Lava] '%s' ohne LavaActor - hebt sich selbst."), *GetName());
	}
}

void ALavaRiseActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Der Anstieg ist Spielzustand und gehoert auf den Server. Die Bewegung der Lavaflaeche
	// erreicht die Clients ueber deren eigene Replikation.
	if (!HasAuthority() || !IsValid(LavaActor))
	{
		return;
	}

	UWorld* Welt = GetWorld();
	if (!Welt || Welt->GetTimeSeconds() < StartDelaySeconds)
	{
		return;
	}

	if (RisenSoFar < MaxRise)
	{
		// Steigen und Pausieren im Wechsel. Die Pause ist Absicht: der Spieler bekommt Zeit zum
		// Umbauen, und der darauf folgende Schub ist dadurch deutlich spuerbar.
		if (RisePhaseSeconds > 0.f)
		{
			PhasenZeit += DeltaSeconds;
			if (bInPause)
			{
				if (PhasenZeit >= AktuellePauseDauer)
				{
					bInPause = false;
					PhasenZeit = 0.f;
					PausenBonus += SpeedBoostPerPause;
				}
			}
			else if (PhasenZeit >= RisePhaseSeconds)
			{
				bInPause = true;
				PhasenZeit = 0.f;
				AktuellePauseDauer = FMath::FRandRange(PauseSecondsMin, FMath::Max(PauseSecondsMin, PauseSecondsMax));
			}
		}

		if (!bInPause)
		{
			// Die Geschwindigkeit waechst mit der bereits verstrichenen STEIGZEIT - Pausen zaehlen
			// nicht mit. Wuerde hier die Weltzeit stehen, haette die Lava Wartezeit und Pausen als
			// Beschleunigungsstrecke geschenkt bekommen und liefe nach jeder Pause davon.
			SteigZeit += DeltaSeconds;
			float Tempo = RiseSpeed + PausenBonus + RiseAcceleration * SteigZeit;
			if (MaxRiseSpeed > 0.f)
			{
				Tempo = FMath::Min(Tempo, MaxRiseSpeed);
			}
			const float Schritt = FMath::Min(Tempo * DeltaSeconds, MaxRise - RisenSoFar);
			FVector Ort = LavaActor->GetActorLocation();
			Ort.Z += Schritt;
			LavaActor->SetActorLocation(Ort);
			RisenSoFar += Schritt;
		}
	}

	ZeitSeitPruefung += DeltaSeconds;
	if (ZeitSeitPruefung < KillCheckInterval)
	{
		return;
	}
	ZeitSeitPruefung = 0.f;

	VerschlungeneToeten(LavaActor->GetActorLocation().Z);
}

void ALavaRiseActor::VerschlungeneToeten(float OberflaechenZ)
{
	UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return;
	}

	const float Schwelle = OberflaechenZ - KillDepth;

	for (TActorIterator<AUnitBase> It(Welt); It; ++It)
	{
		AUnitBase* Einheit = *It;
		if (!IsValid(Einheit))
		{
			continue;
		}

		// Der Bezugspunkt einer Einheit ist ihre KAPSELMITTE, nicht ihr Fuss. Wer die Mitte
		// gegen die Oberflaeche haelt, laesst sie erst sterben, wenn die Lava ihr bis zur
		// Huefte steht - bei einer Standardkapsel rund 88 Einheiten zu spaet. Sichtbare
		// Oberflaeche und Schaden liefen dadurch sichtbar auseinander.
		const float FussZ = Einheit->GetActorLocation().Z - Einheit->GetSimpleCollisionHalfHeight();
		if (FussZ >= Schwelle)
		{
			continue;
		}

		if (OnlyTeamIds.Num() > 0 && !OnlyTeamIds.Contains(Einheit->TeamId))
		{
			continue;
		}

		// Bewusst SetHealth statt KillSilently: die Lava soll die normalen Todeseffekte
		// ausloesen. KillSilently unterdrueckt sie ausdruecklich.
		// DeadEffectsExecuted ist das Flag, das die Einheit selbst gegen doppeltes Sterben
		// setzt - damit wird eine bereits versunkene Einheit nicht jede halbe Sekunde erneut
		// angefasst.
		if (!Einheit->DeadEffectsExecuted)
		{
			Einheit->SetHealth(0.f);
		}
	}
}
