// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassUnitVisualTweenProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassUnitVisualTweenProcessor : public UMassProcessor {
    GENERATED_BODY()

public:
    UMassUnitVisualTweenProcessor();

protected:
    UPROPERTY(EditAnywhere, Category = "Mass")
    bool bLogDebug = false;

    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;
};
