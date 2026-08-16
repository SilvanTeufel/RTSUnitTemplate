// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassEntityQuery.h"
#include "GoToBaseStateProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UGoToBaseStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UGoToBaseStateProcessor();
public:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	// Must match UUnitStateProcessor::ArrivalDistanceMultiplier so client-predicted stop point
	// lands where the server actually halts (server uses this * MovementAcceptanceRadius).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ArrivalDistanceMultiplier = 5.f;

	// --- Crowd settle (server) ---
	// When many workers return to a depleted base they pile up and the outer ones can never reach
	// the tight arrival ring, so they push against the crowd forever (Run-jitter, never Idle).
	// A worker that is already near the base (within ArrivalDistance * this multiplier) but is
	// effectively blocked (2D speed below the threshold for at least MinStateTime in GoToBase) is
	// treated as arrived: it runs the normal ReachedBase path (deposits its load, re-evaluates work,
	// goes Idle when nothing is left). Harmless during active mining (deposit + reassign still fire).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float CrowdSettleRadiusMultiplier = 4.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float CrowdSettleSpeedThreshold = 50.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float CrowdSettleMinStateTime = 1.5f;

	// --- Diagnose #150 (Arbeiter dreht sich mit Ressource auf der Stelle) ---
	// Der Crowd-Settle oben faengt nur Blockaden IN BASISNAEHE ab. Wer unterwegs stecken bleibt,
	// hat keinen Ausgang, weil das MoveTarget hier bewusst nicht mehr nachgefuehrt wird (R105).
	// Diese beiden Werte steuern nur eine Logzeile, sie greifen NICHT ins Verhalten ein.
	// Vor einer Auslieferung wieder entfernen.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float StuckLogSpeedThreshold = 50.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float StuckLogIntervalSeconds = 10.f;

	// --- Pfadende-Settle (Variante b aus #150) ---
	// Gemessen: ein Arbeiter stand 150 s lang 784 von seiner Basis entfernt, aber nur 12
	// Einheiten von seinem eigenen MoveTarget.Center. Der Server klemmt Center auf das
	// Pfadende; reicht der Pfad nicht bis zur Basis, ist die Einheit am Ziel angekommen,
	// das der Zustandsautomat nie als Ankunft akzeptiert. Ein neues Ziel hilft nicht - der
	// Server klemmt sofort wieder. Deshalb derselbe Ausgang wie beim Crowd-Settle.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float PathEndSettleRadius = 150.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float PathEndSettleMinStateTime = 8.f;

private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};