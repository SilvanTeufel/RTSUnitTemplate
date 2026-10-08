// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSUnitTemplateSettings.generated.h"

/**
 * Project Settings -> Plugins -> RTS Unit Template.
 *
 * Values here are saved to DefaultGame.ini and ship with a packaged build, which is the difference to
 * the console variables: those live only for the current session unless someone hand-edits
 * DefaultEngine.ini's [SystemSettings].
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "RTS Unit Template"))
class RTSUNITTEMPLATE_API URTSUnitTemplateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSUnitTemplateSettings();

	/** Shows up under "Plugins" instead of the crowded "Game" section. */
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * Global time dilation applied when a game world starts. 1 = real time, 6 = six times faster.
	 * Used for AI test runs and RL recording: at real time a single useful match takes half an hour.
	 *
	 * The console variable `rts.ai.timescale` still works and OVERRIDES this whenever it is set to
	 * anything other than 1 - handy for trying a value without touching project settings.
	 * Note it is applied at world initialisation, so it takes effect on the NEXT PIE start, not the
	 * running one.
	 */
	UPROPERTY(config, EditAnywhere, Category = "AI|Testing",
	          meta = (DisplayName = "AI Time Scale", ClampMin = "0.1", UIMin = "1.0", UIMax = "20.0"))
	float AITimeScale = 1.f;

	/**
	 * Ladebildschirm fuer Levelwechsel ueber URTSTravelHelpers::TravelToMap.
	 *
	 * Der eingebaute Weg haengt an ACameraControllerBase (eigene Widgetklasse) und faellt
	 * sonst auf den AResourceGameState zurueck. Storylevel laufen aber mit den
	 * Engine-Standardklassen - dort greift beides nicht, und der Spieler sieht bis zum
	 * Ende des Ladens das letzte Bild der alten Karte. Diese Klasse wird in genau dem
	 * Fall benutzt. Leer lassen heisst: kein Ladebildschirm fuer solche Level.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Travel",
	          meta = (DisplayName = "Travel Loading Widget (Fallback)",
	                  AllowedClasses = "/Script/RTSUnitTemplate.LoadingWidget"))
	TSoftClassPtr<class ULoadingWidget> TravelLoadingWidgetClass;

	// ------------------------------------------------------------------------------------------
	// Formation keeping while marching (UUnitFormationSubsystem). All values are read every frame,
	// so changes made here during PIE take effect immediately.
	// ------------------------------------------------------------------------------------------

	/** Units of one move order keep their formation while marching. The console variable rts.formation.enable 0 switches it off as well. */
	UPROPERTY(config, EditAnywhere, Category = "Formation", meta = (DisplayName = "Enable Formation Keeping"))
	bool bFormationEnabled = true;

	/** A move order forms a formation group from this many units on. */
	UPROPERTY(config, EditAnywhere, Category = "Formation", meta = (ClampMin = "2"))
	int32 FormationMinGroupSize = 4;

	/** The group marches at the pace of its slowest unit (like AoE4). Off = every unit uses its own speed as the base. */
	UPROPERTY(config, EditAnywhere, Category = "Formation")
	bool bFormationMatchSlowest = true;

	/**
	 * Highest speed factor for a unit that is CLOSE behind its slot. The allowed boost decays
	 * exponentially with the distance to the slot, from this value down to Boost Far:
	 *   MaxBoost(d) = BoostFar + (BoostNear - BoostFar) * exp(-d / BoostFalloffDistance)
	 * Near zero the response still ramps up (square root over Correction Distance), otherwise a unit
	 * a few centimetres behind would sprint at double speed and overshoot its slot.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "1.0", UIMax = "3.0"))
	float FormationBoostNear = 2.0f;

	/** Speed factor a unit far behind its slot converges to (just below Max Catch-Up Distance). */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "1.0", UIMax = "2.0"))
	float FormationBoostFar = 1.1f;

	/** Distance (cm) over which the boost decays from Boost Near towards Boost Far (exp(-1) = 37 % of the difference left). */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "1.0", Units = "cm"))
	float FormationBoostFalloffDistance = 800.f;

	/** Lowest speed factor for a unit running AHEAD of its slot (0.7 = -30 %). */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float FormationMinSlow = 0.7f;

	/** Error (cm) at which the response ramp reaches full strength. The ramp rises with the square root of the error. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "1.0", Units = "cm"))
	float FormationCorrectionDistance = 200.f;

	/** Errors below this (cm) are ignored - prevents jitter around the slot. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "0.0", Units = "cm"))
	float FormationDeadZone = 15.f;

	/** How fast the speed factor follows its target (per second). Lower = softer. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Speed", meta = (ClampMin = "0.1"))
	float FormationSmoothRate = 8.f;

	/** How strongly a unit steers sideways towards its slot (share across the walking direction, 0.5 = up to ~27 degrees). 0 = speed only. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Lateral", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FormationLateralStrength = 0.5f;

	/** Sideways error (cm) at which full lateral steering is reached. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Lateral", meta = (ClampMin = "1.0", Units = "cm"))
	float FormationLateralDistance = 150.f;

	/** A unit farther than this (cm) behind its slot counts as detached: it does not pull the group centroid back and gets no lateral steering. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float FormationDetachDistance = 1500.f;

	/**
	 * Units farther than this (cm) from their slot are NOT regulated at all - no boost, no brake, no
	 * lateral steering. Keeps units selected from across the map from running the whole way faster (balance).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float FormationMaxCatchUpDistance = 3000.f;

	/** From this distance (cm) of the group centroid to its destination (at least 1.5x the formation radius, max 30 m) units are no longer slowed down. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float FormationFinalDistance = 600.f;

	/** If a unit's move target deviates more than this (cm) from its formation target, it got another order and leaves the group. */
	UPROPERTY(config, EditAnywhere, Category = "Formation|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float FormationTargetTolerance = 600.f;

	/** Convenience accessor; never returns null (UDeveloperSettings are CDO-backed). */
	static const URTSUnitTemplateSettings* Get();
};
