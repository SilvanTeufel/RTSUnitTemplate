// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "Core/RTSUnitUtils.h"
#include "MassEntityQuery.h"
#include "ChaseStateProcessor.generated.h"

struct FMassEntityManager;
struct FMassExecutionContext;
struct FMassStateChaseTag; // Tag für diesen Zustand
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassTransformFragment;
struct FMassCombatStatsFragment;
struct FMassMoveTargetFragment;
struct FMassAgentCharacteristicsFragment;
struct FMassEntityHandle;
struct FMassStatePauseTag; // Zielzustand
struct FMassStateIdleTag; // Zielzustand

UCLASS()
class RTSUNITTEMPLATE_API UChaseStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UChaseStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	void SwitchToPlaceholderState(FMassEntityManager& EntityManager, FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag, AActor* UnitActor);
	// void CalculateRadii(const FMassEntityManager& EntityManager, const FMassEntityHandle TargetEntity,
	//					const FMassAgentCharacteristicsFragment& AttackerChar, const FTransform& AttackerTransform,
	//					const FVector& TargetLocation, float& OutAttackerRadius, float& OutTargetRadius) const;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;

	/**
	 * Anteil der Angriffsreichweite, bei dem Chase als "angekommen" gilt.
	 *
	 * MUSS 1.0 BLEIBEN, solange es keinen zwingenden Grund gibt. Ein Wert < 1 verlangt, dass die
	 * Einheit NAEHER herangeht als ihre Angriffsreichweite - und das kann physisch unmoeglich sein:
	 * die Reichweite wird als AttackRange + Radiensumme gerechnet, bei einem Nahkaempfer mit
	 * Reichweite 200 und Radiensumme 150 fordert 0.9 ganze 35 Einheiten mehr Naehe, die Kollision
	 * und Separation gar nicht zulassen. Die Einheit jagt dann ewig weiter, ohne je anzukommen -
	 * gemeldet als "sie sehen den Gegner, laufen aber nie in Angriffsreichweite".
	 * Das noetige Totband gegen das Hin- und Herkippen sitzt stattdessen auf der Pause-Seite
	 * (PauseRechaseEpsilon), wo es kein Herankommen erzwingt.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate, meta=(ClampMin="0.1", ClampMax="1.0"))
	float ChaseArrivalRangeFactor = 1.0f;

private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};