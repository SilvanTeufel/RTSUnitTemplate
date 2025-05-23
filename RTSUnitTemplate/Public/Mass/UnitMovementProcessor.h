
// Header: UnitMovementProcessor.h
/*
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "UnitMovementProcessor.generated.h"

// Forward Declarations
class UNavigationSystemV1;
struct FMassExecutionContext;
struct FTransformFragment;
struct FMassVelocityFragment; // Nur noch zum Lesen (optional)
struct FMassMoveTargetFragment;
struct FMassNavigationPathFragment; // Pfad-Speicher
struct FMassSteeringFragment;     // Ausgabe für Lenkung
struct FMassAgentRadiusFragment;  // Für Radien
struct FUnitMassTag;
struct FMassReachedDestinationTag; // Signal-Tag

UCLASS()
class RTSUNITTEMPLATE_API UUnitMovementProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UUnitMovementProcessor();

protected:
    virtual void ConfigureQueries() override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;

    // Akzeptanzradius für Wegpunkte (relativ zum Agentenradius?)
    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float PathWaypointAcceptanceRadiusFactor = 1.5f; // Faktor * AgentRadius
};

*/

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

protected:
    // Configuration function called during initialization.
    virtual void ConfigureQueries() override;

    // Execute is called during the processing phase and applies the logic on each entity chunk.
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
    virtual void Initialize(UObject& Owner) override; // Falls du es überschreibst

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ExecutionInterval = 0.1f;

   // void RequestPathfindingAsync(FMassEntityHandle Entity, const FVector& StartLocation, const FVector& EndLocation, ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter); // Verwende TSharedPtr für Filter
    void RequestPathfindingAsync(FMassEntityHandle Entity, FVector StartLocation, FVector EndLocation);

    void ResetPathfindingFlag(FMassEntityHandle Entity);
private:
    FMassEntityQuery EntityQuery;

    float TimeSinceLastRun = 0.0f;
	

    // Add Acceptance Radius if not using the one from MoveTarget
    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float PathWaypointAcceptanceRadius = 100.f; // Example value, adjust as needed

    UPROPERTY(Transient)
    TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
    // Optional: Store NavData pointer if performance is critical
    // TWeakObjectPtr<ANavigationData> CachedNavData = nullptr;
};

