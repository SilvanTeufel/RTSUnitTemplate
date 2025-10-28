#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "Core/UnitData.h"
#include "UnitClientTagSyncProcessor.generated.h"

class AAbilityUnit;

UCLASS()
class RTSUNITTEMPLATE_API UUnitClientTagSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UUnitClientTagSyncProcessor();

	// Toggle debug logging for this processor at runtime
	bool bShowLogs = false;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	// Compute the mirrored enum state for an entity by inspecting replicated Mass tags
	TEnumAsByte<UnitData::EState> ComputeState(const FMassEntityManager& EntityManager, const FMassEntityHandle& Entity) const;

	// Apply to owning actor if it's an AbilityUnit
	void ApplyStateToActor(AAbilityUnit* AbilityUnit, TEnumAsByte<UnitData::EState> NewState) const;
};
