// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassEntityQuery.h"
#include "PreventIdlingProcessor.generated.h"

struct FMassExecutionContext;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassPatrolFragment;
struct FMassPreventIdlingTag;
struct FMassStateIdleTag;
struct FMassStatePatrolIdleTag;

/**
 * Holt herumstehende Einheiten zurueck auf Patrouille.
 *
 * Laeuft ausschliesslich ueber FMassPreventIdlingTag. Das Tag setzt
 * UMassActorBindingComponent beim Erzeugen der Entity, wenn die Spawn-Zeile
 * bPreventIdling gesetzt hat - ab Werk ist das aus, ein Projekt ohne diesen
 * Haken merkt von diesem Prozessor nichts.
 *
 * Es gibt zwei Wege in den Stillstand, und beide loesen sich nicht von allein:
 *  - PatrolIdle: die vorgeschriebene Standpause am Wegpunkt. Sie endet erst,
 *    wenn RandomPatrolMin/MaxIdleTime abgelaufen sind UND der Wuerfel gegen
 *    IdleChance faellt.
 *  - Idle: der Auffangzustand nach dem Kampf. Stirbt das Ziel waehrend einer
 *    Verfolgung, bleibt PlaceholderSignal auf Idle stehen und die Einheit
 *    wartet, bis zufaellig ein neuer Gegner in Sicht laeuft. Fuer diesen Fall
 *    existiert in IdleStateProcessor bereits bSetUnitsBackToPatrol - der Wert
 *    wird beim Binden aber fest auf false gesetzt und ist damit wirkungslos.
 *
 * Umgangen wird nichts: der Prozessor schickt dasselbe Signal PISwitcher, das
 * die Einheit im Normalfall selbst ausgeloest haette. Er kuerzt nur die
 * Wartezeit auf MaxIdleSeconds ab.
 */
UCLASS()
class RTSUNITTEMPLATE_API UPreventIdlingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UPreventIdlingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Taktung des Prozessors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.5f;

	/**
	 * Mindeststandzeit vor dem Eingriff, gemessen an FMassAIStateFragment::StateTimer.
	 *
	 * Steht bewusst auf 0. Der Timer wird bei jedem Eintritt in den Zustand
	 * zurueckgesetzt, und die Einheiten wechseln im Gefecht so oft zwischen den
	 * Zustaenden, dass er praktisch nie ueber Bruchteile einer Sekunde kommt.
	 * Gemessen mit einem Wert von 1.5: jede einzelne gesehene Einheit fiel durch
	 * diese Pruefung, das Feature war damit komplett wirkungslos. Wer hier etwas
	 * groesser als 0 eintraegt, schaltet es also faktisch wieder ab.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate, meta = (ClampMin = "0.0"))
	float MaxIdleSeconds = 0.f;

	/**
	 * Setzt PlaceholderSignal auf PatrolRandom.
	 *
	 * Ohne das behandelt der Prozessor nur das Symptom: die Einheit laeuft los,
	 * faellt nach dem naechsten Kampf aber wieder in denselben Idle, weil der
	 * Platzhalter dort stehengeblieben ist.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool bRepairPlaceholderSignal = true;

	/** Temporaer: schreibt alle 3 s eine Zeile "PREVIDLE ..." mit den Abbruchgruenden. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool bDebugLog = false;

private:
	/** Standpause am Wegpunkt. */
	FMassEntityQuery PatrolIdleQuery;

	/** Auffangzustand nach dem Kampf. */
	FMassEntityQuery IdleQuery;

	float TimeSinceLastRun = 0.0f;

	void RunQuery(FMassEntityQuery& Query, FMassExecutionContext& Context);

	// Diagnose: zaehlt pro Durchlauf, warum eine Einheit nicht angestossen wurde.
	FString DbgReport() const;
	float DbgLogAccu = 0.f;
	int32 DbgSeen = 0;
	int32 DbgSwitching = 0;
	int32 DbgHasTarget = 0;
	int32 DbgNoWaypoint = 0;
	int32 DbgTooEarly = 0;
	int32 DbgSignalled = 0;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};
