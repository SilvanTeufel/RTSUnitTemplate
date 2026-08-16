// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"         // Required for FMassEntityQuery
#include "MassSignalSubsystem.h"
#include "MassEntityQuery.h"
#include "GoToResourceExtractionStateProcessor.generated.h"

// Forward declaration
struct FMassExecutionContext;

/**
 * Processor for units moving towards a resource node to start extraction.
 * Checks for arrival and signals when the unit is close enough.
 */
UCLASS()
class RTSUNITTEMPLATE_API UGoToResourceExtractionStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGoToResourceExtractionStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	// Must match UUnitStateProcessor::ArrivalDistanceMultiplier so client-predicted stop point
	// lands where the server actually halts (server uses this * MovementAcceptanceRadius).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ArrivalDistanceMultiplier = 5.f;

	/**
	 * Ab welcher Abweichung zwischen Bewegungsziel und Ressourcenposition neu gesetzt wird.
	 *
	 * MUSS deutlich groesser als 0 bleiben: `UpdateMoveTarget` ruft intern `CreateNewAction` auf,
	 * jeden Takt aufgerufen startet es die Bewegung endlos neu und der Arbeiter kommt nie los.
	 * Genau deshalb war der Aufruf hier auskommentiert. Ganz weglassen ist aber ebenso falsch -
	 * dann behaelt ein neu zugewiesener Arbeiter das ALTE Ziel. Gemessen: 8 von 19
	 * Stall-Meldungen mit rund 1700 Einheiten Abweichung.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate, meta=(ClampMin="50.0"))
	float WorkerRetargetTolerance = 250.f;

	/**
	 * Vielfaches von `ResourceArrivalDistance`, bis zu dem ein am Pfadende festsitzender
	 * Arbeiter als angekommen gilt.
	 *
	 * Hintergrund: `UnitMovementProcessor.cpp:488` haelt den Arbeiter fuer angekommen, sobald er
	 * `MoveTarget.Center` erreicht - und Center ist dort (Zeile 726) auf das Ende des BEGEHBAREN
	 * Pfades geklemmt. Dieser Prozessor wartet dagegen auf den ECHTEN Knoten. Reicht das Navmesh
	 * nicht nah genug heran, widersprechen sich beide dauerhaft und der Arbeiter steht bis zum
	 * Spielende. Gemessen ueber 2 Laeufe: Medianabstand 294 bei Ankunftsradius 125.
	 *
	 * 3.0 deckt diesen Median mit Reserve ab, ohne dass ein Arbeiter quer ueber die Karte
	 * "ankommt". Der Eingriff greift ohnehin nur nach 8 Sekunden ohne Fortschritt.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate, meta=(ClampMin="1.0"))
	float StuckArrivalFactor = 3.f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};