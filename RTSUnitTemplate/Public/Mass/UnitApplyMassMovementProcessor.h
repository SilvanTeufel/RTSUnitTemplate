// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "UnitApplyMassMovementProcessor.generated.h"

// Forward declarations
struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitApplyMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UUnitApplyMassMovementProcessor();

	// Global logging toggle for this processor
	bool bShowLogs = false;

protected:
    UPROPERTY(EditAnywhere, Category = "Navigation")
    FVector NavMeshProjectionExtent = FVector(100.0f, 100.0f, 500.0f);

    UPROPERTY(EditAnywhere, Category = "Navigation")
    TSubclassOf<UNavigationQueryFilter> FilterClass;

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	FMassEntityQuery ClientEntityQuery;
	
	// Separated execution paths
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
};
