// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BossAoeBlueprintLibrary.generated.h"

class AUnitBase;

/**
 * Zielpunkte fuer die Flaechenfaehigkeiten der Bosse.
 *
 * Bisher haengen alle Boss-AOEs an GetMassActorLocation(Boss) plus einem Zufallsversatz bis
 * hoechstens Range. Wer weiter weg steht als Range, muss deshalb nie ausweichen - der Kampf
 * wird auf Distanz zur Formsache. Diese Bibliothek liefert stattdessen einen Ankerpunkt, der
 * mit einer einstellbaren Wahrscheinlichkeit auf einem Spieler liegt.
 */
UCLASS()
class RTSUNITTEMPLATE_API UBossAoeBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Ankerpunkt fuer eine Flaechenfaehigkeit.
	 *
	 * @param Boss            Die castende Einheit.
	 * @param PlayerChance    Wahrscheinlichkeit (0..1), dass der Anker auf einem Spieler liegt
	 *                        statt auf dem Boss. 0 = altes Verhalten.
	 * @param bFallBackToChasedUnit  Ohne Spieler-Kameraeinheit ersatzweise das aktuelle Ziel
	 *                        des Bosses nehmen, bevor auf den Boss selbst zurueckgefallen wird.
	 * @return Weltposition, um die herum die Flaechen gestreut werden sollen.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Boss",
	          meta = (AdvancedDisplay = "2"))
	static FVector GetAoeAnchorLocation(AUnitBase* Boss, float PlayerChance = 0.6f,
	                                    bool bFallBackToChasedUnit = true);

	/**
	 * Alle gegnerischen Kameraeinheiten (die Figuren, die die Spieler steuern).
	 *
	 * Sie kommen aus AControllerBase::CameraUnitWithTag, nicht aus einer Suche ueber alle
	 * Aktoren - damit trifft es genau die gesteuerten Figuren und keine Begleiteinheit.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Boss")
	static void GetEnemyPlayerUnits(AUnitBase* Boss, TArray<AUnitBase*>& OutUnits);

	/**
	 * Fertiger Spawnpunkt fuer eine einzelne Flaeche - Anker plus Zufallsversatz in einem.
	 *
	 * Ersetzt im Blueprint den ganzen Rechenblock
	 *     BossOrt + ForwardVector(MakeRotator(0,0,Random(0,360))) * Random(Min, Range)
	 * durch einen Knoten. Der Unterschied: liegt der Anker auf einem Spieler, wird nur um
	 * PlayerSpread gestreut - die Flaeche landet also tatsaechlich AUF ihm. Liegt er auf dem
	 * Boss, gilt wie bisher der volle Ring von MinRadius bis Range.
	 *
	 * !! Jeder Aufruf wuerfelt neu. Der Knoten gehoert deshalb INNERHALB der Schleife, sonst
	 * landen alle Flaechen auf demselben Punkt (genau dieser Fehler steckte in SkyFall).
	 *
	 * @param Range        Reichweite der Faehigkeit (Streuradius um den Boss).
	 * @param PlayerChance Wahrscheinlichkeit (0..1) fuer den Anker auf einem Spieler.
	 * @param MinRadius    Mindestabstand, wenn um den Boss gestreut wird.
	 * @param PlayerSpread Streuung um den Spieler herum.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Boss",
	          meta = (AdvancedDisplay = "3"))
	static FVector GetAoeSpawnLocation(AUnitBase* Boss, float Range, float PlayerChance = 0.6f,
	                                   float MinRadius = 250.f, float PlayerSpread = 350.f);
};
