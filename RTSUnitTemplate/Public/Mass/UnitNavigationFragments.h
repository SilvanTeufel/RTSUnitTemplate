// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "MassEntityTypes.h"
#include "NavigationPath.h" // Required for FNavPathSharedPtr
#include "Mass/MassFragmentTraitsOverrides.h"
#include "UnitNavigationFragments.generated.h" // Needs USTRUCT for reflection

USTRUCT()
struct FUnitNavigationPathFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The calculated path the entity is currently following. */
	FNavPathSharedPtr CurrentPath;

	/** The index of the next path point in CurrentPath->GetPathPoints() the entity should move towards. */
	UPROPERTY(Transient) // Transient as path points are runtime data and path object itself isn't UPROPERTY
	int32 CurrentPathPointIndex = 0;

	/** The final destination the path was generated for (useful for checking if target changed). */
	UPROPERTY() // UPROPERTY needed for tracking changes/replication if necessary later
	FVector PathTargetLocation = FVector::ZeroVector;

	UPROPERTY() 
	bool bIsPathfindingInProgress = false;

	/**
	 * Weltzeit, ab der fuer diese Entitaet wieder eine Pfadsuche gestartet werden darf.
	 *
	 * Hintergrund (19.08.): laesst sich das ZIEL nicht auf das Navigationsnetz projizieren, scheitert
	 * FindPathSync mit einem gueltigen Pfadobjekt und NULL Punkten. Der Rueckruf uebernimmt nichts,
	 * die Einheit bleibt ohne Pfad - und im naechsten Takt geht dieselbe Anfrage wieder raus. Zwei
	 * unerreichbare Expansionsmarker im Testlevel kosteten so ~28500 Anfragen je Partie, waehrend die
	 * betroffenen Arbeiter mit vollem Wunschtempo stillstanden.
	 *
	 * Bewusst eine ZEITLICHE Sperre und keine dauerhafte: das Netz aendert sich, wenn Gebaeude fallen
	 * oder eine Energiewand verschwindet - ein einmal unerreichbares Ziel kann spaeter erreichbar sein.
	 * Weltzeit und nicht `static`, damit die Sperre pro Welt gilt und einen PIE-Wechsel nicht ueberlebt.
	 */
	UPROPERTY()
	float NaechsteSucheFruehestens = 0.f;

	/** Reset path data */
	void ResetPath()
	{
		CurrentPath.Reset(); // Clears the shared pointer
		CurrentPathPointIndex = 0;
		PathTargetLocation = FVector::ZeroVector;
	}

	bool HasValidPath() const
	{
		return CurrentPath.IsValid();
	}
};