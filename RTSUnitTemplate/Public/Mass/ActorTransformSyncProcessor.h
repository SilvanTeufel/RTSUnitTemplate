// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h" // For FTransformFragment
#include "Characters/Unit/UnitBase.h"
// Include your hypothetical FMassActorFragment or MassRepresentationFragments if using that system
#include "MassEntityQuery.h"
#include "ActorTransformSyncProcessor.generated.h"

// Forward declaration if needed
class UMassRepresentationSubsystem;

UCLASS()
class RTSUNITTEMPLATE_API UActorTransformSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UActorTransformSyncProcessor();

	// Global logging toggle for this processor
	bool bShowLogs = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float AccumulatedTimeA = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float AccumulatedTimeB = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float VerticalInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float VerticalDeadInterpSpeed = 15.f;
	
	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float VisualISMActorSyncTime = 0.f;
	
	/** Minimale Distanz, die sich die Einheit bewegen muss, damit eine neue Rotation berechnet wird (verhindert Jitter bei Stillstand). */
	UPROPERTY(EditDefaultsOnly, Category = RTSUnitTemplate)
	float MinMovementDistanceForRotation = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Performance Throttling", meta = (ClampMin = "0.001", UIMin="0.001"))
	float MinTickInterval = 0.01f; // Fastest update rate (e.g., at high FPS)
	
	UPROPERTY(EditAnywhere, Category = "Performance Throttling", meta = (ClampMin = "0.01", UIMin="0.01"))
	float MaxTickInterval = 0.5f; // Slowest update rate (e.g., at low FPS)

	// ============================================================================================
	// LUX-ANPASSUNG (28.08.2026) - Rettung, wenn eine Einheit durch die Map faellt.
	// Muss beim Uebernehmen ins Original-Template mitwandern. Siehe REAPPLY_AFTER_PLUGIN_SWAP.md.
	// ============================================================================================

	/**
	 * Sekunden ohne Boden unter der Einheit, nach denen sie an die letzte sichere Position
	 * zurueckgesetzt wird. 0 = Rettung aus.
	 *
	 * Nicht kleiner als etwa 0,5 waehlen: kurze bodenlose Momente sind normal (Kante, Rampe,
	 * Luecke zwischen zwei Meshes) und sollen NICHT zu einem Sprung fuehren.
	 */
	// Config: UMassProcessor ist UCLASS(config = Mass, defaultconfig) - mit dem Config-Specifier
	// stehen diese beiden Werte in Config/DefaultMass.ini unter
	// [/Script/RTSUnitTemplate.ActorTransformSyncProcessor] und lassen sich ohne Neubau aendern,
	// genau wie ExecutionOrder und ProcessingPhase daneben. Ohne Config greift eine Aenderung am
	// Klassen-Default nicht: die laufenden Prozessor-Instanzen lesen ihn nicht neu ein.
	UPROPERTY(config, EditAnywhere, Category = "RTSUnitTemplate|Absturz-Rettung", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float FallRescueAfterSeconds = 1.0f;

	/**
	 * Absolute Hoehe, unter der sofort gerettet wird - ohne auf FallRescueAfterSeconds zu warten.
	 * Wer so tief ist, kommt nicht mehr von allein zurueck.
	 */
	UPROPERTY(config, EditAnywhere, Category = "RTSUnitTemplate|Absturz-Rettung")
	float FallRescueBelowZ = -3000.f;

	// FPS thresholds for interpolation range
	UPROPERTY(EditAnywhere, Category = "Performance Throttling", meta = (ClampMin = "1.0", UIMin="1.0"))
	float LowFPSThreshold = 30.0f; // Below this FPS, use MaxTickInterval
	
	UPROPERTY(EditAnywhere, Category = "Performance Throttling", meta = (ClampMin = "1.0", UIMin="1.0"))
	float HighFPSThreshold = 60.0f; // Above this FPS, use MinTickInterval
		
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	//virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	FMassEntityQuery ClientEntityQuery;
	
	// Cache subsystem pointer for efficiency
	UPROPERTY(Transient)
	UMassRepresentationSubsystem* RepresentationSubsystem; // Example if using Representation Subsystem


	// Separated execution paths
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteRepClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	
	//----------------------------------------------------------------------//
	// HELPER FUNCTIONS for Execute()                                       //
	//----------------------------------------------------------------------//
	
	bool ShouldProceedWithTick(const float FrameDeltaTime, float& OutAccumulatedDeltaTime);
	
	void HandleGroundAndHeight(const AUnitBase* UnitBase, FMassAgentCharacteristicsFragment& CharFragment, const FVector& CurrentActorLocation, const float ActualDeltaTime, FTransform& MassTransform, FVector& InOutFinalLocation, bool bIsDead = false) const;
	
	void RotateTowardsMovement(AUnitBase* UnitBase, const FVector& CurrentVelocity, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FMassAIStateFragment& State, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const;
	
	void RotateTowardsTarget(AUnitBase* UnitBase, FMassEntityManager& EntityManager, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform, bool bPreferEnemy = false) const;
	
	void DispatchPendingUpdates(TArray<FActorTransformUpdatePayload>&& PendingUpdates);

	bool RotateTowardsAbility(AUnitBase* UnitBase, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const;


};