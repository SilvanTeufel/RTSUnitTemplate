// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnergyWallBatchSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Welcher der drei Teile einer Energiewand gezeichnet wird.
 *
 * Die Reihenfolge ist der Index in die Feldreihe unten - nicht umsortieren.
 */
UENUM()
enum class EEnergyWallPart : uint8
{
	TopRod   = 0,
	BottomRod = 1,
	Shield   = 2,
	Anzahl   = 3
};

/**
 * Zeichnet ALLE Energiewaende einer Welt in drei gemeinsamen ISM-Komponenten.
 *
 * Vorher hielt jede Wand drei eigene ISMs mit je EINER Instanz - bei N Waenden also 3N
 * Zeichenaufrufe fuer drei Meshes. Hier gibt es drei ISMs insgesamt, egal wie viele Waende stehen.
 *
 * Zwei Dinge liefen vorher je Komponente und mussten umziehen, weil ein gemeinsamer ISM nur EIN
 * Material hat und Sichtbarkeit nur je Komponente kennt:
 *
 *   - Schildflackern und Nebelsichtbarkeit liefen ueber SetHiddenInGame JE KOMPONENTE. Das gaebe es
 *     je Instanz nicht, also heisst unsichtbar jetzt: Instanzskalierung null. Das kommt OHNE
 *     Materialeingriff aus und passt zum vorhandenen Aussehen - die Wand faehrt beim Auf- und Abbau
 *     ohnehin ueber ihre Y-Skalierung hoch und runter.
 *   - `DespawnStartTime` war ein Skalarparameter auf einem dynamischen Material JE WAND; geteilt
 *     haetten alle Waende dieselbe Aufloesezeit. Der Wert steht jetzt in Custom Data 0.
 *
 * Zum Aufloeseparameter, gemessen am 09.09.2026: WEDER `M_Shield_AH_Inst` NOCH `MI_Emissive_03`
 * kennt einen Parameter `DespawnStartTime`. Der alte Code erzeugte je Wand ein dynamisches Material
 * und schrieb einen Wert hinein, den nie jemand las - ein stiller Leerlauf. Custom Data 0 wird
 * deshalb zwar geschrieben, aber von den heutigen Materialien noch nicht gelesen. Wer das Aufloesen
 * sichtbar machen will, liest im Material `PerInstanceCustomData` Index 0; die Werte liegen bereit.
 *
 * Freiliste statt RemoveInstance: RemoveInstance schiebt die LETZTE Instanz in die entstandene
 * Luecke und macht damit jeden anderswo gemerkten Index ungueltig. Freigegebene Plaetze werden
 * deshalb auf Skalierung null gesetzt und wiederverwendet.
 *
 * Laeuft auf Server UND Client: das Zeichnen ist rein oertlich, jede Seite fuehrt ihre eigenen
 * Instanzen aus dem replizierten Wandzustand.
 */
UCLASS()
class RTSUNITTEMPLATE_API UEnergyWallBatchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEnergyWallBatchSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** Custom-Data-Platz 0: Weltzeit, zu der das Aufloesen begann. Negativ = laeuft nicht. */
	static constexpr int32 CustomDataDespawnStart = 0;

	/**
	 * Custom-Data-Platz 1: Sichtbarkeit, 0 oder 1.
	 *
	 * Wird mitgefuehrt, aber NICHT als Sichtbarkeitsschalter benutzt - das macht die Skalierung
	 * (siehe Kopfkommentar). Der Platz bleibt fuer ein Material, das ausblenden weich gestalten will.
	 */
	static constexpr int32 CustomDataSichtbar = 1;

	/**
	 * Custom-Data-Platz 2: Steigung der Wand je Laengeneinheit (dZ pro uu entlang der lokalen
	 * Y-Achse).
	 *
	 * Das Material baut daraus die Scherung: WorldPositionOffset.Z = Steigung * lokales Y. Damit
	 * bleiben die Seitenkanten der Wand senkrecht und nur Ober- und Unterkante laufen schraeg -
	 * eine gekippte Instanz waere dagegen ein gekipptes Rechteck und stuende schief zu den Tuermen.
	 *
	 * MUSS ueber den Batch laufen: die eigenen ISMs der Wandaktoren sind unsichtbar geschaltet
	 * (SetVisibility(false)), gezeichnet wird ausschliesslich aus dem gemeinsamen Batch-ISM. Werte,
	 * die auf den Aktor-ISMs landen, haben KEINE sichtbare Wirkung - genau daran ist der erste
	 * Anlauf gescheitert.
	 */
	static constexpr int32 CustomDataSteigung = 2;

	/**
	 * Custom-Data-Plaetze 3 und 4: waagerechte Richtung der Wand (X, Y), normiert.
	 *
	 * WOFUER: das Material muss wissen, WIE WEIT ein Vertex entlang der Wand vom Instanzmittelpunkt
	 * entfernt ist. Der naheliegende Weg ueber TransformPosition World->Local funktioniert bei
	 * einem ISM NICHT wie erwartet - "Local" ist dort der KOMPONENTENraum, und der Batch-ISM sitzt
	 * im Weltursprung. Die Rechnung liefert damit faktisch Weltkoordinaten, und die Scherung warf
	 * die Wand weit weg ("im Boden oder in der Luft").
	 *
	 * Mit der Richtung geht es instanzbezogen und damit verlaesslich:
	 *     entlang = dot(WeltPosition - ObjectPositionWS, Richtung)
	 *     WorldPositionOffset.Z = Steigung * entlang
	 * ObjectPositionWS ist bei einem ISM der Mittelpunkt DER INSTANZ - genau der Bezug, der fehlte.
	 */
	static constexpr int32 CustomDataRichtungX = 3;
	static constexpr int32 CustomDataRichtungY = 4;

	static constexpr int32 CustomDataAnzahl = 5;

	/**
	 * Belegt einen Instanzplatz und gibt seinen Index zurueck, oder INDEX_NONE.
	 *
	 * Mesh und Materialien kommen von der ERSTEN Wand, die sich meldet - die Wandklasse setzt sie
	 * im Blueprint, das Plugin kennt sie nicht. Spaetere Waende mit abweichendem Mesh werden
	 * gemeldet und weiter im gemeinsamen ISM gezeichnet; unterschiedliche Meshes je Wand waeren mit
	 * einem Batch grundsaetzlich unvereinbar.
	 */
	int32 BelegePlatz(EEnergyWallPart Teil, const UInstancedStaticMeshComponent* Vorlage,
		const FTransform& WeltTransform);

	/** Setzt Lage und Groesse eines Platzes. WeltTransform, nicht relativ. */
	void SetzeTransform(EEnergyWallPart Teil, int32 Index, const FTransform& WeltTransform);

	/** Setzt die Steigung fuer die Scherung im Material (Custom Data 2). */
	void SetzeSteigung(EEnergyWallPart Teil, int32 Index, float SteigungJeEinheit, const FVector2D& RichtungXY);

	/** Setzt Sichtbarkeit (Custom Data 1). */
	void SetzeSichtbar(EEnergyWallPart Teil, int32 Index, bool bSichtbar);

	/** Setzt den Beginn des Aufloesens (Custom Data 0). Negativ schaltet es ab. */
	void SetzeAufloesebeginn(EEnergyWallPart Teil, int32 Index, float Weltzeit);

	/** Gibt einen Platz zurueck: Skalierung null, Index wandert in die Freiliste. */
	void GibPlatzFrei(EEnergyWallPart Teil, int32 Index);

	/** Wie viele Plaetze gerade belegt sind - fuer die Diagnose. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|EnergyWall")
	int32 GetBelegtePlaetze() const { return BelegteAnzahl; }

	/** Der gemeinsame ISM eines Teils, oder nullptr solange sich keine Wand gemeldet hat. */
	const UInstancedStaticMeshComponent* GibISM(EEnergyWallPart Teil) const
	{
		const int32 i = static_cast<int32>(Teil);
		return (i >= 0 && i < static_cast<int32>(EEnergyWallPart::Anzahl)) ? ISMs[i].Get() : nullptr;
	}

	/** Wie viele Plaetze eines Teils in der Freiliste auf Wiederverwendung warten. */
	int32 GibFreieAnzahl(EEnergyWallPart Teil) const
	{
		const int32 i = static_cast<int32>(Teil);
		return (i >= 0 && i < static_cast<int32>(EEnergyWallPart::Anzahl)) ? FreieIndizes[i].Num() : 0;
	}

private:
	UInstancedStaticMeshComponent* HoleOderBaue(EEnergyWallPart Teil,
		const UInstancedStaticMeshComponent* Vorlage);

	UPROPERTY()
	TObjectPtr<AActor> BatchAktor = nullptr;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ISMs[static_cast<int32>(EEnergyWallPart::Anzahl)] = { nullptr, nullptr, nullptr };

	TArray<int32> FreieIndizes[static_cast<int32>(EEnergyWallPart::Anzahl)];

	int32 BelegteAnzahl = 0;

	/** Einmal melden, wenn eine Wand ein anderes Mesh mitbringt als das zuerst gesehene. */
	bool bMeshAbweichungGemeldet[static_cast<int32>(EEnergyWallPart::Anzahl)] = { false, false, false };
};
