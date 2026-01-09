// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"     // FTransformFragment
#include "MassEntitySubsystem.h"
#include "MassMovementFragments.h"  // FMassVelocityFragment, FMassMoveTargetFragment
#include "MassNavigationFragments.h" // FUnitNavigationPathFragment (Assumes this exists from previous step)
#include "UnitNavigationFragments.h"
#include "UnitMovementProcessor.generated.h"

// Forward Declarations
class UNavigationSystemV1;
class UWorld;
struct FMassExecutionContext;

UCLASS()
class RTSUNITTEMPLATE_API UUnitMovementProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UUnitMovementProcessor();

    // Global logging toggle for this processor
    bool bShowLogs = false;

protected:
    // Configuration function called during initialization.
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;

    // Execute is called during the processing phase and applies the logic on each entity chunk.
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

    UPROPERTY(EditAnywhere, Category = "Navigation")
    FVector NavMeshProjectionExtent = FVector(100.0f, 100.0f, 500.0f);

    UPROPERTY(EditAnywhere, Category = "Navigation")
    TSubclassOf<UNavigationQueryFilter> FilterClass;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;
    
    void RequestPathfindingAsync(FMassEntityHandle Entity, FVector StartLocation, FVector EndLocation);
    void ResetPathfindingFlagDeferred(FMassEntityHandle Entity);

private:
    FMassEntityQuery EntityQuery;
	FMassEntityQuery ClientEntityQuery;
    
    
    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float PathWaypointAcceptanceRadius = 100.f; // Example value, adjust as needed

	UPROPERTY(Transient)
    TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
    
    void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
    void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
};

