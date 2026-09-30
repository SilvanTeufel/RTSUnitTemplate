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
 * WAS DIESER PROZESSOR TUT, je Durchlauf:
 *  1. Er liest die AKTUELLE Geometrie jeder aktiven Wand (Mittelpunkt, Halbmasse, Drehung) aus
 *     der NavObstacleBox. Die Box wird zur Laufzeit neu vermessen (RegisterObstacle), deshalb
 *     wird sie gelesen und nicht zwischengespeichert.
 *  2. Er prueft je Einheit, ob sie im Quader steckt - in dessen LOKALEM Raum, damit schraege
 *     Waende genauso behandelt werden wie achsparallele.
 *  3. Steckt sie drin, wird der Gameplay-Effekt angewandt (Freund oder Feind je TeamId), mit
 *     einer Sperrzeit je Einheit, damit nicht jeder Durchlauf erneut auslaest.
 *  4. Und sie wird ueber die naechstgelegene Seitenflaeche hinausgeschoben, waehrend die Kraft
 *     IN die Wand hinein geloescht wird. Genau das verhindert das Durchdruecken durch andere
 *     Einheiten: der Trennschub des UnitSeparationProcessor summiert sich in enger Formation
 *     und schiebt Einheiten sonst quer durch die Wand. Dieser Prozessor laeuft deshalb NACH
 *     der Avoidance-Gruppe - er sieht den fertigen Schub und kann ihn zuruecknehmen.
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

	/** Zeichnet die gelesenen Quader und die Gegenkraefte. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = false;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Taktung. 0 laesst den Prozessor in jedem Bild laufen. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExecutionInterval = 0.05f;

	/**
	 * Wie weit eine Einheit ueber die Wandflaeche hinaus gesetzt wird, zusaetzlich zu ihrem
	 * eigenen Radius. Zu klein, und sie rutscht im naechsten Durchlauf wieder hinein; zu gross,
	 * und sie springt sichtbar.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float PushOutMargin = 10.f;

	/**
	 * Staerke der Gegenkraft, mit der eine Einheit aus der Wand geschoben wird. Wirkt zusaetzlich
	 * zum harten Versetzen und haelt Einheiten aussen, die dauerhaft dagegen gedrueckt werden.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float CounterForce = 4000.f;

	/** Sperrzeit je Einheit und Wand, bevor der Gameplay-Effekt erneut angewandt wird. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float EffectReapplyInterval = 1.0f;

private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.f;

	/** Letzter Anwendungszeitpunkt je Einheit und Wand, fuer EffectReapplyInterval. */
	TMap<TPair<FMassEntityHandle, TWeakObjectPtr<AActor>>, double> LastEffectTime;
};
