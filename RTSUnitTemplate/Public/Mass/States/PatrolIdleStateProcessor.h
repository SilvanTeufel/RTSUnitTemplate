#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "PatrolIdleStateProcessor.generated.h"

// Forward Decls...
struct FMassExecutionContext;
struct FMassStatePatrolIdleTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassPatrolFragment;
struct FMassVelocityFragment;
struct FMassStatePatrolRandomTag;
struct FMassStateChaseTag;


UCLASS()
class RTSUNITTEMPLATE_API UPatrolIdleStateProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UPatrolIdleStateProcessor();
protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EntityQuery;
};