// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h" // For FTransformFragment
#include "Characters/Unit/UnitBase.h"
// Include your hypothetical FMassActorFragment or MassRepresentationFragments if using that system
#include "ActorTransformSyncProcessor.generated.h"

// Forward declaration if needed
class UMassRepresentationSubsystem;

UCLASS()
class RTSUNITTEMPLATE_API UActorTransformSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UActorTransformSyncProcessor();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float AccumulatedTimeA = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float AccumulatedTimeB = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	float VerticalInterpSpeed = 10.f;
	/** Minimale Distanz, die sich die Einheit bewegen muss, damit eine neue Rotation berechnet wird (verhindert Jitter bei Stillstand). */
	UPROPERTY(EditDefaultsOnly, Category = RTSUnitTemplate)
	float MinMovementDistanceForRotation = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Performance Throttling", meta = (ClampMin = "0.001", UIMin="0.001"))
	float MinTickInterval = 0.01f; // Fastest update rate (e.g., at high FPS)

	UPROPERTY(EditAnywhere, Category = "Performance Throttling", meta = (ClampMin = "0.01", UIMin="0.01"))
	float MaxTickInterval = 0.5f; // Slowest update rate (e.g., at low FPS)

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

	// Cache subsystem pointer for efficiency
	UPROPERTY(Transient)
	UMassRepresentationSubsystem* RepresentationSubsystem; // Example if using Representation Subsystem


    //----------------------------------------------------------------------//
    // HELPER FUNCTIONS for Execute()                                       //
    //----------------------------------------------------------------------//
	
    bool ShouldProceedWithTick(const float ActualDeltaTime);
	
    void HandleGroundAndHeight(const AUnitBase* UnitBase, FMassAgentCharacteristicsFragment& CharFragment, const FVector& CurrentActorLocation, const float ActualDeltaTime, FTransform& MassTransform, FVector& InOutFinalLocation) const;
	
    void RotateTowardsMovement(AUnitBase* UnitBase, const FVector& CurrentVelocity, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FMassAIStateFragment& State, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const;
	
    void RotateTowardsTarget(AUnitBase* UnitBase, FMassEntityManager& EntityManager, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const;
	
    void DispatchPendingUpdates(TArray<FActorTransformUpdatePayload>&& PendingUpdates);

	void RotateTowardsAbility(AUnitBase* UnitBase, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassAgentCharacteristicsFragment& Char, const FVector& CurrentActorLocation, float ActualDeltaTime, FTransform& InOutMassTransform) const;


};