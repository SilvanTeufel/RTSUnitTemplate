// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonTypes.h"
#include "MassSignalSubsystem.h"
#include "IdleStateProcessor.generated.h"

// Forward declare Fragments and Tags used
struct FMassAITargetFragment;
struct FMassAIStateFragment;
struct FMassCombatStatsFragment;
struct FMassPatrolFragment;
struct FMassStateTimerFragment;
struct FMassUnitPathFragment;
struct FMassStateIdleTag;
struct FMassStateChaseTag;
struct FMassStateRunTag;
struct FMassStatePauseTag;
struct FMassStatePatrolRandomTag;
struct FMassStatePatrolIdleTag;
struct FMassStateCastingTag;
struct FMassStateIsAttackedTag;
struct FMassStateGoToBaseTag;
struct FMassStateGoToBuildTag;
struct FMassStateBuildTag;
struct FMassStateGoToResourceExtractionTag;
struct FMassStateResourceExtractionTag;
struct FMassStateDetectTag;
struct FMassVelocityFragment;
struct FTransformFragment;


UCLASS()
class RTSUNITTEMPLATE_API UIdleStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UIdleStateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	void SwitchToChaseState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag);
	void SwitchToPauseState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag);
	void SwitchToRunState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag);
	void SwitchToPatrolRandomState(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassAIStateFragment& StateFrag);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	bool bFollowTickThisFrame = false;
	
	// --- Konfigurationswerte ---
	// Besser: Diese Werte aus einem Shared Fragment lesen (z.B. FMassAIConfigSharedFragment)
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bSetUnitsBackToPatrol = true;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float SetUnitsBackToPatrolTime = 3.f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};