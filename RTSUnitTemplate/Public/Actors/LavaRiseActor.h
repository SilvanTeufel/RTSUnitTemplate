// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LavaRiseActor.generated.h"

class AUnitBase;

/**
 * Laesst eine vorhandene Lavaflaeche nach einer Wartezeit langsam steigen und toetet alles,
 * was sie erreicht.
 *
 * Der Aktor bewegt nicht selbst Geometrie, sondern hebt einen ANDEREN Aktor an (LavaActor,
 * im Level zugewiesen). Das ist Absicht: die Lava in Level_7 ist eine gepoolte ISM mit vielen
 * Instanzen - den Traegeraktor anzuheben verschiebt alle Instanzen auf einen Schlag, waehrend
 * ein Nachbauen der Instanzen deren Anordnung verloren haette.
 */
UCLASS()
class RTSUNITTEMPLATE_API ALavaRiseActor : public AActor
{
	GENERATED_BODY()

public:
	ALavaRiseActor();

	/** Der Aktor, dessen Z angehoben wird (die Lavaflaeche). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava")
	TObjectPtr<AActor> LavaActor = nullptr;

	/** Erst nach so vielen Sekunden Spielzeit setzt der Anstieg ein. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float StartDelaySeconds = 240.f;

	/** ANFANGS-Anstieg in Unreal-Einheiten je Sekunde. Mit RiseAcceleration waechst er von hier aus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float RiseSpeed = 12.f;

	/** Zunahme der Steiggeschwindigkeit je Sekunde (uu/s^2). 0 = gleichfoermig wie bisher.
	 *  Gezaehlt wird ab dem EINSETZEN des Anstiegs, nicht ab Levelstart - sonst haette die Lava
	 *  ihre Beschleunigung schon waehrend der Wartezeit angesammelt und liefe sofort los. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float RiseAcceleration = 0.f;

	/** Obergrenze fuer die Steiggeschwindigkeit. 0 = keine Grenze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float MaxRiseSpeed = 0.f;

	/** Laenge einer Steigphase, bevor die Lava eine Pause einlegt. 0 = durchgehend, keine Pausen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float RisePhaseSeconds = 0.f;

	/** Kuerzeste Pause in Sekunden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float PauseSecondsMin = 30.f;

	/** Laengste Pause in Sekunden. Jede Pause wird zwischen Min und Max ausgewuerfelt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float PauseSecondsMax = 40.f;

	/** Aufschlag auf die Steiggeschwindigkeit nach JEDER Pause (uu/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float SpeedBoostPerPause = 0.f;

	/** Laeuft gerade eine Pause? Im Spiel ablesbar. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lava")
	bool bInPause = false;

	/** Summe aller bisherigen Pausen-Aufschlaege. RiseSpeed selbst bleibt unangetastet, damit der
	 *  im Editor gesetzte Wert nicht zur Laufzeit verfaelscht wird. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lava")
	float PausenBonus = 0.f;

	/** Bisher verstrichene Steigzeit - im Spiel ablesbar, um die Beschleunigung zu pruefen. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lava")
	float SteigZeit = 0.f;

	/** Wie weit die Lava insgesamt steigen darf. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.0"))
	float MaxRise = 900.f;

	/** Abstand zwischen zwei Toetungsdurchlaeufen. Jeden Takt ueber alle Einheiten zu gehen
	 *  waere Verschwendung - die Lava steigt langsam genug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava", meta = (ClampMin = "0.05"))
	float KillCheckInterval = 0.5f;

	/** Wie tief der FUSS einer Einheit unter der Oberflaeche liegen muss, um als verschlungen
	 *  zu gelten. 0 = sie stirbt, sobald die sichtbare Lava ihre Fuesse erreicht.
	 *
	 *  Frueher wurde die Kapselmitte verglichen und zusaetzlich um 40 gepuffert - zusammen rund
	 *  128 Einheiten, um die der Schaden der sichtbaren Oberflaeche hinterherlief. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava")
	float KillDepth = 0.f;

	/** Nur Einheiten dieser Teams treffen. Leer = alle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava")
	TArray<int32> OnlyTeamIds;

	/** Bereits zurueckgelegter Anstieg - im Spiel sichtbar, damit man den Stand pruefen kann. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lava")
	float RisenSoFar = 0.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	float ZeitSeitPruefung = 0.f;
	float PhasenZeit = 0.f;
	float AktuellePauseDauer = 0.f;

	/** Toetet alle Einheiten, die unter der Lavaoberflaeche liegen. */
	void VerschlungeneToeten(float OberflaechenZ);
};
