#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "PatrolRandomStateProcessor.generated.h"

// Forward Decls...
struct FMassExecutionContext;
struct FMassStatePatrolRandomTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassPatrolFragment;
struct FMassMoveTargetFragment;
struct FMassStatePatrolIdleTag;
struct FMassStateChaseTag;
class UNavigationSystemV1; // Für Random Point

UCLASS()
class RTSUNITTEMPLATE_API UPatrolRandomStateProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UPatrolRandomStateProcessor();
protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EntityQuery;
	void SetNewRandomPatrolTarget(FMassPatrolFragment& PatrolFrag, FMassMoveTargetFragment& MoveTarget, class AUnitBase* UnitBaseActor, UNavigationSystemV1* NavSys, UWorld* World); // Helper
};