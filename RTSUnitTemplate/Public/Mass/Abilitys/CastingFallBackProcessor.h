// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassEntityQuery.h"
#include "CastingFallBackProcessor.generated.h"

USTRUCT()
struct FMassCastingFallbackTag : public FMassTag
{
    GENERATED_BODY()
};

UCLASS()
class RTSUNITTEMPLATE_API UCastingFallBackProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UCastingFallBackProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
    
    UFUNCTION()
    void HandleCastingFallback(FName SignalName, TArray<FMassEntityHandle>& Entities);

    UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;

private:
    FMassEntityQuery EntityQuery;
    float TimeSinceLastRun = 0.0f;

    UPROPERTY(Transient)
    TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};
