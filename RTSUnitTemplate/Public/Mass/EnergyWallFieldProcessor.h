// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassEntityQuery.h"
#include "EnergyWallFieldProcessor.generated.h"

struct FMassExecutionContext;

/**
 * Traegt die EnergyWall in die Mass-Welt ein - beides, was die Kollision der NavObstacleBox
 * bisher haette leisten sollen und nicht konnte.
 *
 * WARUM UEBERHAUPT: Einheiten haben KEINE Kollision (Nutzervorgabe, steht in den Blueprints;
 * nur die Heldeneinheit hat QueryOnly). Ein OnComponentBeginOverlap auf der NavObstacleBox
 * feuert deshalb nie - der Effekt beim Beruehren der Wand wurde nie angewandt, und das
 * Durchdruecken konnte niemand aufhalten. Overlap-Abfragen sind in diesem Projekt generell
 * kein taugliches Werkzeug fuer Einheiten; dieselbe Falle gab es schon bei der CC-Erkennung.
 *
 * WAS DIESER PROZESSOR TUT, je Bild (seit 01.10.2026, vorher alle 0.05 s):
 *  1. Er liest die AKTUELLE Geometrie jeder aktiven Wand: die beiden Turmmittelpunkte als
 *     Strecke in XY, die echte halbe Wanddicke (Box-X-Halbmass) und die Hoehe aus der
 *     NavObstacleBox. Die Box wird zur Laufzeit neu vermessen (RegisterObstacle), deshalb wird
 *     sie gelesen und nicht zwischengespeichert.
 *  2. Er misst je Einheit den Abstand zur STRECKE (Kapsel um die Wand). Hinter einem Turm klemmt
 *     der naechste Punkt auf den Turmmittelpunkt, die Abstossrichtung wird dort radial und
 *     bleibt stetig. Frueher war das Feld die gepolsterte Box: sie ragte 200 uu ueber jeden Turm
 *     hinaus, die Richtung war die Box-Seite mit der kleinsten Durchdringung (Vorzeichenwechsel
 *     auf der Mittellinie, Sprung auf der Diagonalen), dazu hartes Versetzen um Tiefe+10 uu. Die
 *     Angreifer eines Turms wurden vom Trennschub in diese Klinge gedrueckt und sprangen im
 *     Saegezahn - das Zittern an den Pfeilern.
 *  3. Bei Beruehrung wird der Gameplay-Effekt angewandt (Freund oder Feind je TeamId), mit
 *     einer Sperrzeit je Einheit, damit nicht jeder Durchlauf erneut ausloest.
 *  4. Im Band vor der Wand (ContactBand) wird der Anteil von Geschwindigkeit, Kraft und
 *     Sollgeschwindigkeit, der IN die Wand zeigt, anteilig herausgenommen - die Einheit gleitet
 *     an der Wand entlang. Das verhindert auch das Durchdruecken durch andere Einheiten: der
 *     Trennschub summiert sich in enger Formation und schob Einheiten sonst quer durch die Wand.
 *     Der Prozessor laeuft deshalb NACH Separation, SoftAvoidance und MovingAvoidance - er sieht
 *     den fertigen Schub und kann ihn zuruecknehmen.
 *  5. Steckt die Einheit trotzdem drin, schiebt eine gedaempfte Feder sie hinaus; hart versetzt
 *     wird nur noch, was ueber HardCorrectionSlop hinaus drinsteckt.
 *
 * Damit kann die Kollisionslogik der Box abgeschaltet werden; fuer das Navigationsnetz bleibt
 * sie unveraendert zustaendig (NavModifier + CanEverAffectNavigation).
 */
UCLASS()
class RTSUNITTEMPLATE_API UEnergyWallFieldProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UEnergyWallFieldProcessor();

	/** Zeichnet die Wandstrecken und die Abstossrichtungen (rot = steckt drin). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = false;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/**
	 * Taktung. 0 laesst den Prozessor in jedem Bild laufen - und das ist seit 01.10.2026 der
	 * Standard. Feder und Gleiten wirken auf Kraft und Geschwindigkeit, die jedes Bild neu
	 * entstehen; liefe der Prozessor nur jedes dritte Bild, drueckte der Trennschub dazwischen
	 * ungebremst in die Wand.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExecutionInterval = 0.f;

	/**
	 * Zuschlag auf die halbe Wanddicke (Box-X-Halbmass, 2.5-5 uu). Die Kontaktdistanz ist
	 * halbe Wanddicke + Zuschlag + Kapselradius der Einheit.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExtraHalfThickness = 0.f;

	/**
	 * Breite des Bandes vor der Kontaktdistanz, in dem der Anteil in die Wand hinein anteilig
	 * entfernt wird (0 am Bandrand, voll bei Beruehrung). 0 = nur bei Beruehrung, hart.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ContactBand = 40.f;

	/**
	 * Federsteifigkeit gegen die Durchdringung (Kraft je uu). Wird pro Bild auf den stabilen
	 * Hoechstwert begrenzt (halbe Durchdringung je Schritt), ein zu grosser Wert schiesst also
	 * nicht ueber.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float SpringStiffness = 200.f;

	/** Daempfung der Bewegung nach aussen, solange die Einheit drinsteckt. Ebenfalls begrenzt. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float SpringDamping = 10.f;

	/**
	 * Durchdringung, die die Feder allein aufloest. Nur was darueber hinaus drinsteckt, wird
	 * hart versetzt (und nur um diesen Rest) - der Notfall, nicht der Normalfall.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float HardCorrectionSlop = 15.f;

	/** Abstand vor der Kontaktdistanz, ab dem der Gameplay-Effekt als Beruehrung zaehlt. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float EffectContactMargin = 10.f;

	/**
	 * Unterhalb dieses Abstands zur Strecke gibt die Geometrie keine Richtung mehr vor; dann
	 * entscheidet die Geschwindigkeit (die Seite, von der die Einheit kam), sonst die positive
	 * Senkrechte.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float NormalEpsilon = 0.5f;

	/**
	 * Auch die Sollgeschwindigkeit an der Wand entlanglenken. Ohne das laeuft die Einheit weiter
	 * hinein, und UnitApplyMassMovementProcessor lenkt die Gegenfeder (gegen die Laufrichtung)
	 * seitlich um - die Einheit rutscht dann seitwaerts statt stehenzubleiben.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	bool bSlideDesiredVelocity = true;

	/** Sperrzeit je Einheit und Wand, bevor der Gameplay-Effekt erneut angewandt wird. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float EffectReapplyInterval = 1.0f;

private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.f;

	/** Letzter Anwendungszeitpunkt je Einheit und Wand, fuer EffectReapplyInterval. */
	TMap<TPair<FMassEntityHandle, TWeakObjectPtr<AActor>>, double> LastEffectTime;
};
