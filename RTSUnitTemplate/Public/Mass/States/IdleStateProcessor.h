#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonTypes.h"
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
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	// --- Konfigurationswerte ---
	// Besser: Diese Werte aus einem Shared Fragment lesen (z.B. FMassAIConfigSharedFragment)
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bSetUnitsBackToPatrol = true;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float SetUnitsBackToPatrolTime = 3.f;
};