// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassResourcePlacementProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassResourcePlacementProcessor : public UMassProcessor {
    GENERATED_BODY()

public:
    UMassResourcePlacementProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;
};
