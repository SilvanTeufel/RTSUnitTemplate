// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassUnitPlacementProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassUnitPlacementProcessor : public UMassProcessor {
    GENERATED_BODY()

public:
    UMassUnitPlacementProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;
};
