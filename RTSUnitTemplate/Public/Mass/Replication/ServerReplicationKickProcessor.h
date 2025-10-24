#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ServerReplicationKickProcessor.generated.h"

/**
 * Minimal server-only processor that ensures UMassUnitReplicatorBase::ProcessClientReplication
 * is executed each tick for eligible chunks. Intended as a diagnostic/fallback to
 * verify replication flow. Can be removed once MassReplicationProcessor path is confirmed.
 */
UCLASS()
class RTSUNITTEMPLATE_API UServerReplicationKickProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UServerReplicationKickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
