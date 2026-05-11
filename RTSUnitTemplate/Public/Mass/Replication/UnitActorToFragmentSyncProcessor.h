// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "GameModes/ResourceGameMode.h"
#include "UnitActorToFragmentSyncProcessor.generated.h"

class AUnitBase;
class AEffectArea;
struct FMassEntityManager;
struct FMassEntityHandle;
struct FMassCombatStatsFragment;
struct FMassAgentCharacteristicsFragment;
struct FMassAIStateFragment;
struct FMassVisibilityFragment;
struct FMassPatrolFragment;
struct FEffectAreaImpactFragment;
struct FTransformFragment;

/**
 * Processor to synchronize data from AUnitBase actors to Mass fragments locally on both Server and Client.
 * This allows reducing the network payload by not replicating data that is already replicated via the Actor system.
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitActorToFragmentSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UUnitActorToFragmentSyncProcessor();

	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
	


private:
	void SyncCombatStats(const AUnitBase& Unit, FMassCombatStatsFragment& CombatStatsFragment);
	void SyncCharacteristics(const AUnitBase& Unit, FMassAgentCharacteristicsFragment& CharacteristicsFragment);
	void SyncAIState(const AUnitBase& Unit, FMassAIStateFragment& AIStateFragment, FMassCombatStatsFragment& CombatStatsFragment);
	void SyncVisibility(const AUnitBase& Unit, FMassVisibilityFragment& VisibilityFragment);
	void SyncPatrol(const AUnitBase& Unit, FMassPatrolFragment& PatrolFragment, FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle);
	void SyncEffectArea(const AEffectArea& Area, FEffectAreaImpactFragment& ImpactFragment, FMassCombatStatsFragment* CombatStatsFragment, FMassAgentCharacteristicsFragment* CharacteristicsFragment, FTransformFragment* TransformFragment);
};
