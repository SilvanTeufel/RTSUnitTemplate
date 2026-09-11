// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "UnitApplyMassMovementProcessor.generated.h"

// Forward declarations
struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitApplyMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UUnitApplyMassMovementProcessor();

	// Global logging toggle for this processor
	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool bShowLogs = false;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	float SoftAvoidanceZExtent = 5000.f;

	// --------------------------------------------------------------------------------------------
	// Patt-Aufloesung an Hindernissen.
	//
	// Gemessen am 29.08.2026 ueber 1130 Beobachtungen: 98 % der Einheiten, die trotz gesetzter
	// Sollgeschwindigkeit nicht vom Fleck kommen, haben eine Ausweichkraft, die fast exakt gegen
	// ihre Laufrichtung zeigt (mittlerer Kosinus -0,96) und mit Staerke 275 gegen Solltempo 450
	// ausreicht, um die Bewegung zu neutralisieren. Beide gehen in dieselbe Summe
	// (AccelInput + Ausweichkraft), also bleibt netto null - die Einheit tritt auf der Stelle,
	// typischerweise vor einem Gebaeude auf dem Weg zur Basis oder zum Abbauplatz.
	//
	// Die Korrektur laesst die STAERKE der Ausweichkraft unangetastet - Einheiten sollen weiterhin
	// nicht in Hindernisse laufen - und dreht nur ihre RICHTUNG: der bremsende Anteil wird auf
	// Bremsanteil gestutzt und seitlich umgelenkt, sodass die Einheit um das Hindernis herumlaeuft
	// statt dagegen zu druecken.
	// --------------------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate|Ausweichen")
	bool bTangentialAvoidance = true;

	/** Anteil der frontal bremsenden Ausweichkraft, der erhalten bleibt. 0 = gar nicht bremsen. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate|Ausweichen", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AvoidanceBrakeFactor = 0.25f;

	/** Dreht eine bremsende Ausweichkraft seitlich um. Siehe Kommentar oben. */
	FVector RedirectedAvoidanceForce(const FVector& Ausweichkraft, const FVector& Sollgeschwindigkeit) const;
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	FMassEntityQuery ClientEntityQuery;
	
	// Separated execution paths
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	// --------------------------------------------------------------------------------------------
	// Standwache (Diagnose): letzte bekannte Position je Einheit mit Zeitstempel.
	//
	// Damit laesst sich zaehlen, wie viele Einheiten laenger stillstehen - auch solche ohne
	// Bewegungswillen, die der Blockiert-Zaehler nicht sieht. Nur der Serverzweig schreibt hier,
	// und der laeuft auf dem GameThread (bRequiresGameThreadExecution), also ohne Sperre.
	// --------------------------------------------------------------------------------------------
	struct FStationaryWatch
	{
		FVector Ort = FVector::ZeroVector;
		double Zeitpunkt = 0.0;
	};
	TMap<FMassEntityHandle, FStationaryWatch> StationaryWatches;

	// Gebaeudeorte fuer die Abstandsmessung der Haenger. Einmal je Sekunde eingesammelt -
	// ein TActorIterator je Takt waere zu teuer, und die Gebaeude bewegen sich nicht.
	TArray<FVector> BuildingLocations;
	double BuildingLocationsStamp = -1.0;
};
