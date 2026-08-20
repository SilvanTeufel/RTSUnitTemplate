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
#include "NavFilters/NavigationQueryFilter.h"
#include "MassEntityQuery.h"
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
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;

    // Execute is called during the processing phase and applies the logic on each entity chunk.
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

    UPROPERTY(EditAnywhere, Category = "Navigation")
    FVector NavMeshProjectionExtent = FVector(100.0f, 100.0f, 500.0f);
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;
    
    // Zaehler fuer die Diagnose in RequestPathfindingAsync: wie oft ein brauchbarer Teilpfad
    // verworfen wird, weil die Einheit nicht in einer Energiewand steht. Bewusst KEIN static -
    // ein prozesslanger Zaehler ueberlebt den Wechsel der PIE-Sitzung und verfaelscht die Messung.
    // Geteilter Zeiger, weil die Pfadsuche auf einem Hintergrund-Thread laeuft: die Lambda erfasst
    // eine Kopie des Zeigers und haelt den Zaehler am Leben, ohne `this` einzufangen.
    TSharedPtr<FThreadSafeCounter, ESPMode::ThreadSafe> AusweichVerwurfZaehler;

    void RequestPathfindingAsync(FMassEntityHandle Entity, FVector StartLocation, FVector EndLocation);
    void ResetPathfindingFlagDeferred(FMassEntityHandle Entity);

private:
    FMassEntityQuery EntityQuery;
	FMassEntityQuery ClientEntityQuery;
    
    
    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float PathWaypointAcceptanceRadius = 100.f; // Example value, adjust as needed

    /**
     * Ab welcher Zielverschiebung ein NEUER Pfad angefordert wird.
     *
     * Vorher wurde exakt verglichen (`PathTargetLocation != FinalDestination`). Beim Verfolgen
     * eines Gegners verschiebt sich das Ziel jeden Takt, der Vergleich war also praktisch immer
     * wahr - jede Runde neue Pfadsuche, und solange die laeuft, wird die Geschwindigkeit auf null
     * gesetzt ("kein blindes Vorwaertslaufen"). Die Einheit stand damit dauerhaft still, obwohl
     * ihr MoveTarget volles Tempo trug: gemessen SollTempo 900 bei Versatz 0, Pfad nie ueber zwei
     * Punkte hinaus. Das war die Ursache von "Kampfeinheiten laufen auf der Stelle".
     *
     * 250 ist bewusst grosszuegig: kleiner als jede sinnvolle Angriffsreichweite der Fernkaempfer
     * und gross genug, dass ein normal laufender Gegner nicht bei jedem Schritt neu planen laesst.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta=(ClampMin="0.0"))
    float PathRetargetTolerance = 250.f;

	UPROPERTY(Transient)
    TObjectPtr<UMassEntitySubsystem> EntitySubsystem;

    FSharedConstNavQueryFilter CachedStrictFilter;
    
    void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
    void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
};

