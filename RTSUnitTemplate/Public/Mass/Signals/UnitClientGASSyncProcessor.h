#pragma once

#include "MassProcessor.h"
#include "UnitClientGASSyncProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UUnitClientGASSyncProcessor : public UMassProcessor
{
    GENERATED_BODY()
public:
    UUnitClientGASSyncProcessor();
protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

    FMassEntityQuery EntityQuery;
};
