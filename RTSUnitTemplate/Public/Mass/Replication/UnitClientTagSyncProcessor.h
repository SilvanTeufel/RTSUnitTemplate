// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "MassEntityTypes.h"
#include "Core/UnitData.h"
#include "MassEntityQuery.h"
#include "UnitClientTagSyncProcessor.generated.h"

class AAbilityUnit;
struct FMassSignalNameLookup;

UCLASS()
class RTSUNITTEMPLATE_API UUnitClientTagSyncProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()
public:
	UUnitClientTagSyncProcessor();

	// Toggle debug logging for this processor at runtime
	bool bShowLogs = false;

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

private:
	FMassEntityQuery EntityQuery;

	// Eigene Abfrage fuer die Signalbehandlung.
	//
	// EntityQuery schliesst NeedsInitialKick und Frozen aus - fuer den Taktbetrieb ist das
	// richtig, fuer ein EINMALIGES Signal aber toedlich: UnitSpawned feuert genau einmal, und
	// wer es verpasst, bekommt es nie wieder. Traegt die Entitaet in diesem Frame noch den
	// Kick-Tag, faellt sie aus der Abfrage und bleibt auf dem Client ohne Zustand und ohne Tags.
	FMassEntityQuery SpawnSignalQuery;
	FMassEntityQuery InitialKickCleanupQuery;
	FMassEntityQuery LoadingTagCleanupQuery;

	TEnumAsByte<UnitData::EState> ComputeState(const FMassEntityManager& EntityManager, const FMassEntityHandle& Entity, const struct FMassAIStateFragment* StateFrag, const struct FMassCombatStatsFragment* CombatStatsFrag, class AUnitBase* UnitBase) const;

	// Apply to owning actor if it's an AbilityUnit
	void ApplyStateToActor(AAbilityUnit* AbilityUnit, TEnumAsByte<UnitData::EState> NewState) const;

	void HandleUnitSpawned(FMassEntityHandle Entity, FMassEntityManager& EntityManager);
};
