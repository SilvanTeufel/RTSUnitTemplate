// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonTypes.h"
#include "MassSignalSubsystem.h"
#include "IdleStateProcessor.generated.h"

// Forward declare Fragments and Tags used
struct FMassStateIdleTag;
struct FMassVelocityFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassPatrolFragment;
struct FMassStateTimerFragment;
struct FMassStateChaseTag;
struct FMassStatePatrolTag;
// struct FMassStateEvasionTag; // Falls Evasion hier ausgelöst wird


UCLASS()
class RTSUNITTEMPLATE_API UIdleStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UIdleStateProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 0.1f;
	
private:
	FMassEntityQuery EntityQuery;

	float TimeSinceLastRun = 0.0f;

	// --- Konfigurationswerte ---
	// Besser: Diese Werte aus einem Shared Fragment lesen (z.B. FMassAIConfigSharedFragment)
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bSetUnitsBackToPatrol = true;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float SetUnitsBackToPatrolTime = 3.f;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};